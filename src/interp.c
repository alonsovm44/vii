#include "vii.h"
#include <math.h>
#include <time.h>

/* ──────────────────────── Table ──────────────────────── */

static unsigned hash(const char *s) {
    unsigned h = 0;
    for (int i = 0; s[i]; i++) h = h * 31 + (unsigned char)s[i];
    return h % TABLE_SIZE;
}

Table *table_new(Table *parent) {
    Table *t = arena_alloc(global_arena, sizeof(Table));
    t->parent = parent;
    return t;
}

Value *table_get(Table *t, const char *key) {
    for (Table *cur = t; cur; cur = cur->parent) {
        unsigned h = hash(key);
        for (Entry *e = cur->buckets[h]; e; e = e->next) {
            /* Use pointer comparison first for interned keys, fallback to strcmp */
            if (e->key == key || strcmp(e->key, key) == 0) return e->val;
        }
    }
    return NULL;
}

/* Updates an existing variable in the scope chain if found. Handles VAL_REF aliasing. */
static bool table_update(Table *t, const char *key, Value *val) {
    for (Table *cur = t; cur; cur = cur->parent) {
        unsigned h = hash(key);
        for (Entry *e = cur->buckets[h]; e; e = e->next) {
            if (strcmp(e->key, key) == 0) {
                if (e->is_constant) {
                    fprintf(stderr, COL_RED "Runtime error:" COL_RST " cannot reassign constant '%s'\n", key);
                    exit(1);
                }
                if (e->val && e->val->kind == VAL_REF) {
                    *(e->val->u.as_ptr) = *val;
                } else {
                    e->val = val;
                }
                return true;
            }
        }
    }
    return false;
}

void table_set(Table *t, const char *key, Value *val) {
    unsigned h = hash(key);
    for (Entry *e = t->buckets[h]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            if (e->is_constant) {
                fprintf(stderr, COL_RED "Runtime error:" COL_RST " cannot reassign constant '%s'\n", key);
                fflush(stderr);
                exit(1);
            }
            e->val = val;
            return;
        }
    }
    Entry *e = arena_alloc(global_arena, sizeof(Entry));
    e->key = arena_intern(global_arena, key);
    e->val = val;
    e->is_constant = is_all_caps(key);
    e->next = t->buckets[h];
    t->buckets[h] = e;
}

void table_free(Table *t) {
    /* No-op: handled by arena */
}

/* ──────────────────────── Function registry ──────────────────────── */

static Func *funcs = NULL;

typedef struct EntDef {
    char *name;
    Node *def;
    struct EntDef *next;
} EntDef;

static EntDef *ents = NULL;

static void ent_register(const char *name, Node *def) {
    EntDef *e = arena_alloc(global_arena, sizeof(EntDef));
    e->name = arena_intern(global_arena, name);
    e->def = def;
    e->next = ents;
    ents = e;
}

static EntDef *ent_find(const char *name) {
    for (EntDef *e = ents; e; e = e->next)
        if (strcmp(e->name, name) == 0) return e;
    return NULL;
}

static void func_register(const char *name, Node *def) {
    Func *f = arena_alloc(global_arena, sizeof(Func));
    f->name = arena_intern(global_arena, name);
    f->def = def;
    f->next = funcs;
    funcs = f;
}

static Func *func_find(const char *name) {
    for (Func *f = funcs; f; f = f->next)
        if (strcmp(f->name, name) == 0) return f;
    return NULL;
}

Value *val_dict(void) {
    Value *v = arena_alloc(global_arena, sizeof(Value));
    v->kind = VAL_DICT;
    v->u.as_ent.fields = table_new(NULL);
    return v;
}

Value *val_break(void) {
    Value *v = arena_alloc(global_arena, sizeof(Value));
    v->kind = VAL_BREAK;
    return v;
}

Value *val_skip(void) {
    Value *v = arena_alloc(global_arena, sizeof(Value));
    v->kind = VAL_SKIP;
    return v;
}

Value *val_ent(const char *type_name) {
    Value *v = arena_alloc(global_arena, sizeof(Value));
    v->kind = VAL_ENT;
    v->u.as_ent.type_name = (char*)type_name;
    v->u.as_ent.fields = table_new(NULL);
    return v;
}

Value *val_uni(const char *type_name) {
    Value *v = arena_alloc(global_arena, sizeof(Value));
    v->kind = VAL_UNI;
    v->u.as_ent.type_name = (char*)type_name;
    v->u.as_ent.fields = table_new(NULL);
    return v;
}

static void runtime_error(Node *n, const char *fmt, ...) {
    fprintf(stderr, COL_RED "Runtime error" COL_RST);
    if (n && n->filename) {
        fprintf(stderr, " in %s on line %d", n->filename, n->line);
    }
    fprintf(stderr, ": ");
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    fflush(stderr);
    exit(1);
}

/* ──────────────────────── CLI args ──────────────────────── */

Value *cli_args = NULL;

/* ──────────────────────── Eval ──────────────────────── */

static Value *val_unwrap(Value *v) {
    int depth = 0;
    while (v) {
        if (v->kind == VAL_REF) {
            v = v->u.as_ptr;
        } else if (v->kind == VAL_OUT) {
            v = v->u.as_inner;
        } else {
            break;
        }
        if (depth++ > 100) { fprintf(stderr, "Runtime error: circular reference or signal loop detected\n"); exit(1); }
    }
    return v;
}

/* Defer stack for cleanup operations */
#define MAX_DEFER_DEPTH 64
static Node *defer_stack[MAX_DEFER_DEPTH];
static int defer_stack_depth = 0;

static void defer_push(Node *n) {
    if (defer_stack_depth >= MAX_DEFER_DEPTH) {
        fprintf(stderr, "Runtime error: defer stack overflow\n");
        exit(1);
    }
    defer_stack[defer_stack_depth++] = n;
}

static void defer_execute_to_depth(Table *env, int target_depth) {
    while (defer_stack_depth > target_depth) {
        Node *deferred = defer_stack[--defer_stack_depth];
        eval(deferred, env);
    }
}

Value *eval_block(Node *block, Table *env) {
    Value *last = val_none();
    int defer_start = defer_stack_depth;
    for (int i = 0; i < block->body_count; i++) {
        last = eval(block->body[i], env); // Evaluate each statement
        if (last->kind == VAL_BREAK || last->kind == VAL_OUT || last->kind == VAL_SKIP) {
            defer_execute_to_depth(env, defer_start);
            break; // Stop on control flow signals
        }
    }
    defer_execute_to_depth(env, defer_start);
    return last;
}

Value *eval(Node *n, Table *env) {
    if (!n) return val_none();

    switch (n->kind) {
        case ND_NUM:   return val_num(n->num);
        case ND_STR:   return val_str(n->str);
        case ND_NOT: {
            Value *v = eval(n->left, env);
            return val_bit(!val_truthy(v));
        }
        case ND_OUT: {
            Value *v = arena_alloc(global_arena, sizeof(Value));
            v->kind = VAL_OUT;
            v->u.as_inner = eval(n->left, env);
            return v;
        }
        case ND_UMINUS: {
            Value *v = val_unwrap(eval(n->left, env));
            if (v->kind != VAL_NUM) {
                runtime_error(n, "unary minus requires a number");
            }
            return val_num(0 - v->u.as_num);
        }
        case ND_BITNOT: {
            Value *v = val_unwrap(eval(n->left, env));
            if (v->kind != VAL_NUM) {
                runtime_error(n, "bitwise NOT requires a number");
            }
            return val_num(~(long long)v->u.as_num);
        }
        case ND_VAR:   {
            Value *v = table_get(env, n->name);
            extern int trace;
            if (!v) {
                Func *fn = func_find(n->name);
                if (fn) {
                    Table *scope = table_new(env);
                    Value *res = eval_block(fn->def->left, scope);
                    table_free(scope);
                    if (res->kind == VAL_OUT) return res->u.as_inner;
                    return res;
                }
                runtime_error(n, "undefined variable '%s'", n->name);
            }
            return v;
        }
        case ND_SKIP: {
            return val_skip();
        }
        case ND_DEFER: {
            defer_push(n->left);
            return val_none();
        }
        case ND_ASSIGN: {
            Value *val;
            if (n->right->kind == ND_VAR && ent_find(n->right->name)) {
                EntDef *ed = ent_find(n->right->name);
                if (ed->def->kind == ND_ENT_DEF) {
                    val = val_ent(ed->name);
                } else {
                    val = val_uni(ed->name);
                }
                /* Entities initialize all fields to none; unions just need the field table */
                for (int i = 0; i < ed->def->body_count; i++)
                    table_set(val->u.as_ent.fields, ed->def->body[i]->name, val_none());
            } else {
                val = eval(n->right, env);
            }

            if (n->left->kind == ND_VAR) {
                if (!table_update(env, n->left->name, val)) {
                    table_set(env, n->left->name, val);
                }
            } else if (n->left->kind == ND_DEREF) { // Assignment to a dereferenced pointer: valof x = ...
                Value *p_val = val_unwrap(eval(n->left->left, env)); // Get the pointer value
                if (p_val->kind != VAL_PTR) { runtime_error(n, "assignment to non-pointer dereference"); }
                if (!p_val->u.as_ptr) { runtime_error(n, "attempt to assign to null pointer dereference"); }

                Value *target_val = p_val->u.as_ptr; // The Value struct being pointed to
                *target_val = *val;
                return val;
            } else if (n->left->kind == ND_AT) {
                Value *list = val_unwrap(eval(n->left->left, env));
                Value *idx  = val_unwrap(eval(n->left->right, env));
                if (list->kind != VAL_LIST) { fprintf(stderr, "Runtime error: 'at' on non-list\n"); exit(1); }
                int i = (int)idx->u.as_num;
                if (i < 0) i += list->u.as_list.item_count;
                /* For fixed arrays, enforce bounds strictly (no appending) */
                if (list->u.as_list.fixed_cap > 0) {
                    if (i < 0 || i >= list->u.as_list.item_count) {
                        fprintf(stderr, "Runtime error: index %d out of bounds for fixed array of size %d\n", i, list->u.as_list.item_count);
                        exit(1);
                    }
                    list->u.as_list.items[i] = val;
                } else {
                    /* Dynamic list: allow appending */
                    if (i == list->u.as_list.item_count) {
                        if (list->u.as_list.item_count >= list->u.as_list.item_cap) {
                            val_list_grow(list);
                        }
                        list->u.as_list.items[list->u.as_list.item_count++] = val;
                    } else {
                        if (i < 0 || i >= list->u.as_list.item_count) { fprintf(stderr, "Runtime error: index %d out of range\n", i); exit(1); }
                        list->u.as_list.items[i] = val;
                    }
                }
            } else if (n->left->kind == ND_MEMBER) {
                Value *obj = val_unwrap(eval(n->left->left, env));
                if (obj->kind != VAL_ENT && obj->kind != VAL_UNI) runtime_error(n, "'..' on non-entity/union");
                EntDef *ed = ent_find(obj->u.as_ent.type_name);
                bool found = false;
                for(int i=0; i < ed->def->body_count; i++) if(!strcmp(ed->def->body[i]->name, n->left->name)) found = true;
                if (!found) runtime_error(n, "%s '%s' has no field '%s'", 
                    obj->kind == VAL_ENT ? "entity" : "union", obj->u.as_ent.type_name, n->left->name);
                
                if (obj->kind == VAL_UNI) {
                    for (int i = 0; i < TABLE_SIZE; i++) obj->u.as_ent.fields->buckets[i] = NULL;
                }
                table_set(obj->u.as_ent.fields, n->left->name, val);
                return val;
            }
            return val;
        }
        case ND_HEAP_ALLOC: {
            Value *size_val = val_unwrap(eval(n->left, env));
            if (size_val->kind != VAL_NUM) runtime_error(n, "heap_alloc requires a numeric size");
            // Allocate a Value struct on the heap and initialize it to VAL_NONE
            Value *new_val = (Value*)malloc(sizeof(Value));
            if (!new_val) runtime_error(n, "heap_alloc: out of memory");
            memset(new_val, 0, sizeof(Value));
            new_val->kind = VAL_NONE;
            return val_ptr(new_val);
        }
        case ND_HEAP_FREE: {
            Value *p_val = val_unwrap(eval(n->left, env));
            if (p_val->kind != VAL_PTR) runtime_error(n, "heap_free requires a pointer");
            if (p_val->u.as_ptr) {
                free(p_val->u.as_ptr);
                p_val->u.as_ptr = NULL; // Prevent double free
            }
            return val_none();
        }

        case ND_SIZEOF: {
            if (n->type_tag) {
                const char *t = n->type_tag;
                if (!strcmp(t, "i8") || !strcmp(t, "u8")) return val_num(1);
                if (!strcmp(t, "i16") || !strcmp(t, "u16")) return val_num(2);
                if (!strcmp(t, "i32") || !strcmp(t, "u32") || !strcmp(t, "f32")) return val_num(4);
                if (!strcmp(t, "i64") || !strcmp(t, "u64") || !strcmp(t, "f64")) return val_num(8);
                if (strstr(t, "ptr") == t || !strcmp(t, "ref")) return val_num(8);
                /* If it's a generic Vii type, return the size of a boxed Value */
                return val_num(sizeof(Value));
            }
            /* sizeof expression: returns the total memory footprint of the value */
            Value *v = val_unwrap(eval(n->left, env));
            size_t size = sizeof(Value);
            if (v->kind == VAL_STR && v->u.as_str) {
                size += strlen(v->u.as_str) + 1;
            } else if (v->kind == VAL_LIST && v->u.as_list.items) {
                size += v->u.as_list.item_cap * sizeof(Value*);
            } else if (v->kind == VAL_DICT && v->u.as_ent.fields) {
                size += sizeof(Table);
                for (int i = 0; i < TABLE_SIZE; i++) {
                    for (Entry *e = v->u.as_ent.fields->buckets[i]; e; e = e->next) {
                        size += sizeof(Entry);
                    }
                }
            }
            return val_num((double)size);
        }

        case ND_FOR: {
            Value *list = val_unwrap(eval(n->left, env));
            if (list->kind != VAL_LIST) { 
                fprintf(stderr, "Runtime error: for requires a list\n");
                fprintf(stderr, "This error occurs when you try to iterate over a non-list value. In Vii for loops are reserved to lists for simplicity.");
                 exit(1); 
                }
            for (int i = 0; i < list->u.as_list.item_count; i++) {
                table_set(env, n->name, list->u.as_list.items[i]);
                Value *res = eval_block(n->body[0], env);
                if (res->kind == VAL_BREAK) break;
                if (res->kind == VAL_SKIP) continue; // Continue to next iteration
                if (res->kind == VAL_OUT) return res;
            }
            return val_none();
        }
        case ND_SPLIT: {
            Value *str = val_unwrap(eval(n->left, env));
            Value *delim = val_unwrap(eval(n->right, env));
            if (str->kind != VAL_STR || delim->kind != VAL_STR) return val_list();
            Value *res = val_list();
            char *s = strdup(str->u.as_str);
            char *token = strtok(s, delim->u.as_str);
            while (token) {
                if (res->u.as_list.item_count >= res->u.as_list.item_cap) {
                    val_list_grow(res);
                }
                res->u.as_list.items[res->u.as_list.item_count++] = val_str(token);
                token = strtok(NULL, delim->u.as_str);
            }
            free(s);
            return res;
        }
        case ND_TRIM: {
            Value *v = val_unwrap(eval(n->left, env));
            if (v->kind != VAL_STR) return v;
            char *s = v->u.as_str;
            while(isspace((unsigned char)*s)) s++;
            if(*s == 0) return val_str("");
            char *end = s + strlen(s) - 1;
            while(end > s && isspace((unsigned char)*end)) end--;
            *(end+1) = 0;
            return val_str(s);
        }
        case ND_REPLACE: {
            Value *v = val_unwrap(eval(n->left, env));
            Value *target = val_unwrap(eval(n->body[0], env));
            Value *repl = val_unwrap(eval(n->body[1], env));
            if (v->kind != VAL_STR || target->kind != VAL_STR || repl->kind != VAL_STR) return v;
            
            char *s = v->u.as_str;
            char *t = target->u.as_str;
            char *r = repl->u.as_str;
            size_t t_len = strlen(t);
            if (t_len == 0) return v;

            char buffer[4096];
            char *p = s;
            char *out = buffer;
            while (*p) {
                char *found = strstr(p, t);
                if (found == p) {
                    strcpy(out, r);
                    out += strlen(r);
                    p += t_len;
                } else {
                    *out++ = *p++;
                }
            }
            *out = '\0';
            return val_str(buffer);
        }
        case ND_SAFE: {
            /* Placeholder for v1.3 error recovery. 
               Currently just evaluates the branch. */
            return eval(n->left, env);
        }
        case ND_ENT_DEF: {
        case ND_UNI_DEF:
            ent_register(n->name, n);
            return val_none();
        }
        case ND_TYPESET_DEF: {
            // Type aliases are handled at parse-time for type checking.
            // At runtime, they just register their definition.
            ent_register(n->name, n);
            return val_none();
        }
        case ND_INDEX_DEF: {
            if (n->name) ent_register(n->name, n);
            double current_val = 0;
            for (int i = 0; i < n->body_count; i++) {
                Node *item = n->body[i];
                Value *v;
                if (item->left) {
                    v = eval(item->left, env);
                    if (v->kind == VAL_NUM) current_val = v->u.as_num;
                } else {
                    v = val_num(current_val);
                }
                table_set(env, item->name, v);
                current_val += 1.0;
            }
            return val_none();
        }
        case ND_MEMBER: {
            Value *obj = val_unwrap(eval(n->left, env));
            if (obj->kind != VAL_ENT && obj->kind != VAL_UNI) runtime_error(n, "'..' on non-entity/union");
            Value *v = table_get(obj->u.as_ent.fields, n->name);
            if (!v) runtime_error(n, "entity '%s' has no field '%s'", obj->u.as_ent.type_name, n->name);
            return v;
        }
        case ND_BINOP: {
            /* short-circuit for and/or */
            if (n->op == TOK_AND) {
                Value *l = eval(n->left, env);
                if (!val_truthy(l)) return val_bit(0);
                Value *r = eval(n->right, env);
                return val_bit(val_truthy(r));
            }
            if (n->op == TOK_OR) {
                Value *l = eval(n->left, env);
                if (val_truthy(l)) return val_bit(1);
                Value *r = eval(n->right, env);
                return val_bit(val_truthy(r));
            }
            Value *l = val_unwrap(eval(n->left, env));
            Value *r = val_unwrap(eval(n->right, env));
            if (l->kind == VAL_STR && r->kind == VAL_STR) {
                int cmp = strcmp(l->u.as_str, r->u.as_str);
                switch (n->op) {
                    case TOK_LT:  return val_bit(cmp < 0);
                    case TOK_GT:  return val_bit(cmp > 0);
                    case TOK_LTE: return val_bit(cmp <= 0);
                    case TOK_GTE: return val_bit(cmp >= 0);
                    default: break;
                }
            }
            double a = l->u.as_num, b = r->u.as_num;
            if (n->op == TOK_PLUS && (l->kind == VAL_STR || r->kind == VAL_STR)) {
                char la[512], ra[512];
                if (l->kind == VAL_STR) snprintf(la, sizeof(la), "%s", l->u.as_str);
                else snprintf(la, sizeof(la), "%g", l->u.as_num);
                if (r->kind == VAL_STR) snprintf(ra, sizeof(ra), "%s", r->u.as_str);
                else snprintf(ra, sizeof(ra), "%g", r->u.as_num);
                size_t total_len = strlen(la) + strlen(ra) + 1;
                char *combined = arena_alloc(global_arena, total_len);
                memcpy(combined, la, strlen(la));
                memcpy(combined + strlen(la), ra, strlen(ra) + 1);
                Value *res = val_num(0); res->kind = VAL_STR; res->u.as_str = combined;
                return res;
            }
            switch (n->op) {
                case TOK_PLUS:  return val_num(a + b);
                case TOK_MINUS: return val_num(a - b);
                case TOK_STAR:  return val_num(a * b);
                case TOK_SLASH: return val_num(b != 0 ? a / b : 0);
                case TOK_PCT:   return val_num(b != 0 ? fmod(a, b) : 0);
                case TOK_LT:    return val_bit(a < b);
                case TOK_GT:    return val_bit(a > b);
                case TOK_LTE:   return val_bit(a <= b);
                case TOK_GTE:   return val_bit(a >= b);
                case TOK_NE:    {
                    if (l->kind == VAL_STR && r->kind == VAL_STR)
                        return val_bit(strcmp(l->u.as_str, r->u.as_str) != 0);
                    return val_bit(a != b);
                }
                case TOK_EQEQ:  {
                    if (l->kind == VAL_STR && r->kind == VAL_STR)
                        return val_bit(strcmp(l->u.as_str, r->u.as_str) == 0);
                    return val_bit(a == b);
                }
                case TOK_BITAND: return val_num((long long)a & (long long)b);
                case TOK_BITOR:  return val_num((long long)a | (long long)b);
                case TOK_BITXOR: return val_num((long long)a ^ (long long)b);
                case TOK_LSHIFT: return val_num((long long)a << (long long)b);
                case TOK_RSHIFT: return val_num((long long)a >> (long long)b);
                default: return val_num(0);
            }
        }
        case ND_IF: {
            Value *cond = eval(n->left, env);
            if (val_truthy(cond)) {
                if (n->body_count > 0) return eval_block(n->body[0], env);
            } else if (n->right) {
                if (n->right->kind == ND_BLOCK) {
                    return eval_block(n->right, env);
                }
                return eval(n->right, env);
            }
            return val_none();
        }
        case ND_WHILE: {
            while (val_truthy(eval(n->left, env))) {
                Value *res = eval_block(n->body[0], env);
                if (res->kind == VAL_BREAK) break;
                if (res->kind == VAL_SKIP) continue; // Continue to next iteration
                if (res->kind == VAL_OUT) return res;
            }
            return val_none();
        }
        case ND_BREAK: {
            return val_break();
        }
        case ND_DO: {
            func_register(n->name, n);
            return val_none();
        }
        case ND_REF: {
            if (n->left->kind != ND_VAR) { fprintf(stderr, "Runtime error: ref requires a variable\n");
                fprintf(stderr, "The 'ref' operator can only be applied to variables. If you want to take the address of a more complex expression, consider using 'addr' instead.\n");                
                exit(1); }
            Value *v = table_get(env, n->left->name);
            if (!v) { fprintf(stderr, "Runtime error: undefined variable '%s'\n", n->left->name); exit(1); }
            return val_ref(v);
        }
        case ND_ADDR: { // Address-of operator
            if (n->left->kind != ND_VAR) { runtime_error(n, "addr requires a variable"); }
            Value *v = table_get(env, n->left->name);
            if (!v) { runtime_error(n, "undefined variable '%s'", n->left->name); }
            return val_ptr(v);
        }
        case ND_PTR_ADD: { // Pointer addition: ptr-add ptr offset
            Value *ptr_val = val_unwrap(eval(n->left, env));
            if (ptr_val->kind != VAL_PTR) { runtime_error(n, "ptr-add requires a pointer"); }
            Value *offset_val = val_unwrap(eval(n->right, env));
            if (offset_val->kind != VAL_NUM) { runtime_error(n, "ptr-add requires numeric offset"); }
            /* Calculate new pointer: add offset * sizeof(Value) bytes */
            Value **base = (Value**)ptr_val->u.as_ptr;
            int offset = (int)offset_val->u.as_num;
            return val_ptr((Value*)(base + offset));
        }
        case ND_PTR_SUB: { // Pointer subtraction: ptr-sub ptr offset
            Value *ptr_val = val_unwrap(eval(n->left, env));
            if (ptr_val->kind != VAL_PTR) { runtime_error(n, "ptr-sub requires a pointer"); }
            Value *offset_val = val_unwrap(eval(n->right, env));
            if (offset_val->kind != VAL_NUM) { runtime_error(n, "ptr-sub requires numeric offset"); }
            /* Calculate new pointer: subtract offset * sizeof(Value) bytes */
            Value **base = (Value**)ptr_val->u.as_ptr;
            int offset = (int)offset_val->u.as_num;
            return val_ptr((Value*)(base - offset));
        }
        case ND_DEREF: { // Dereference operator
            Value *p_val = val_unwrap(eval(n->left, env));
            if (p_val->kind != VAL_PTR) { runtime_error(n, "valof requires a pointer"); }
            if (!p_val->u.as_ptr) { runtime_error(n, "attempt to dereference null pointer"); }
            return p_val->u.as_ptr;
        }
        case ND_ASK: {
            char buf[1024];
            if (!fgets(buf, sizeof(buf), stdin)) return val_str("");
            buf[strcspn(buf, "\n")] = '\0';
            char *end;
            double d = strtod(buf, &end);
            if (*end == '\0' && end != buf) return val_num(d);
            return val_str(buf);
        }
        case ND_ASKFILE: {
            Value *path = eval(n->left, env);
            if (path->kind != VAL_STR) { fprintf(stderr, "Runtime error: ask requires a string path\n"); exit(1); }
            FILE *f = fopen(path->u.as_str, "rb");
            if (!f) return val_str("");
            fseek(f, 0, SEEK_END);
            long len = ftell(f);
            fseek(f, 0, SEEK_SET);
            char *buf = malloc(len + 1);
            fread(buf, 1, len, f);
            buf[len] = '\0';
            fclose(f);
            Value *v = val_str(buf);
            free(buf);
            return v;
        }
        case ND_LIST: {
            return val_list();
        }
        case ND_DICT: {
            return val_dict();
        }
        case ND_KEYS: {
            Value *dict = val_unwrap(eval(n->left, env));
            if (dict->kind != VAL_DICT) { fprintf(stderr, "Runtime error: keys requires a dict\n"); exit(1); }
            Value *res = val_list();
            for (int i = 0; i < TABLE_SIZE; i++) {
                for (Entry *e = dict->u.as_ent.fields->buckets[i]; e; e = e->next) {
                    /* Grow list capacity if necessary */
                    if (res->u.as_list.item_count >= res->u.as_list.item_cap) {
                        val_list_grow(res);
                    }
                    res->u.as_list.items[res->u.as_list.item_count++] = val_str(e->key);
                }
            }
            return res;
        }
        case ND_KEY: {
            Value *dict = val_unwrap(eval(n->left, env));
            Value *key = val_unwrap(eval(n->body[0], env));
            Value *val = eval(n->body[1], env);
            if (dict->kind != VAL_DICT) { runtime_error(n, "'key' on non-dict"); }
            if (key->kind != VAL_STR) { runtime_error(n, "dict key must be string"); }
            table_set(dict->u.as_ent.fields, key->u.as_str, val);
            return val;
        }
        case ND_AT: {
            Value *list = val_unwrap(eval(n->left, env));
            Value *idx  = val_unwrap(eval(n->right, env));
            if (list->kind == VAL_DICT) {
                if (idx->kind != VAL_STR) { runtime_error(n, "dict index must be string"); }
                Value *v = table_get(list->u.as_ent.fields, idx->u.as_str);
                return v ? v : val_none();
            }
            if (list->kind == VAL_STR) {
                int i = (int)idx->u.as_num;
                if (i < 0) i += (int)strlen(list->u.as_str);
                if (i < 0 || i >= (int)strlen(list->u.as_str)) return val_none();
                char buf[2] = { list->u.as_str[i], '\0' };
                return val_str(buf);
            }
            if (list->kind != VAL_LIST) { 
                fprintf(stderr, "Runtime error: 'at' on non-list (kind=%d, file=%s, line=%d)\n", 
                    list->kind, n->filename ? n->filename : "unknown", n->line); 
                exit(1); 
            }
            int i = (int)idx->u.as_num;
            if (i < 0) i += list->u.as_list.item_count;
            if (i < 0 || i >= list->u.as_list.item_count) return val_none();
            return list->u.as_list.items[i];
        }
        case ND_SET: {
            Value *list = val_unwrap(eval(n->left, env));
            if (list->kind != VAL_LIST) { fprintf(stderr, "Runtime error: 'set' on non-list\n"); exit(1); }
            Value *idx  = val_unwrap(eval(n->body[0], env));
            Value *val  = eval(n->body[1], env);
            int i = (int)idx->u.as_num;
            if (i < 0) i += list->u.as_list.item_count;
            /* For fixed arrays, enforce bounds strictly (no appending) */
            if (list->u.as_list.fixed_cap > 0) {
                if (i < 0 || i >= list->u.as_list.item_count) {
                    fprintf(stderr, "Runtime error: index %d out of bounds for fixed array of size %d\n", i, list->u.as_list.item_count);
                    exit(1);
                }
                list->u.as_list.items[i] = val;
            } else {
                /* Dynamic list: allow appending */
                if (i == list->u.as_list.item_count) {
                    if (list->u.as_list.item_count >= list->u.as_list.item_cap) {
                        val_list_grow(list);
                    }
                    list->u.as_list.items[list->u.as_list.item_count++] = val;
                } else {
                    if (i < 0 || i >= list->u.as_list.item_count) { fprintf(stderr, "Runtime error: index %d out of range\n", i); exit(1); }
                    list->u.as_list.items[i] = val;
                }
            }
            return val;
        }
        case ND_PUT: {
            Value *path = eval(n->left, env);
            Value *data = eval(n->right, env);
            if (path->kind != VAL_STR) { runtime_error(n, "put requires a string path"); }
            FILE *f;
            if (path->u.as_str[0] == '\0') f = stdout;
            else f = fopen(path->u.as_str, (n->op == TOK_APPEND) ? "a" : "w");
            if (!f) { runtime_error(n, "cannot open '%s' for writing", path->u.as_str); }
            if (data->kind == VAL_STR) fprintf(f, "%s", data->u.as_str);
            else val_print_to(data, f);
            if (f != stdout) fclose(f);
            return val_none();
        }
        case ND_ARG: {
            return cli_args;
        }
        case ND_LEN: {
            Value *v = eval(n->left, env);
            if (v->kind == VAL_STR) return val_num((double)strlen(v->u.as_str));
            if (v->kind == VAL_LIST) return val_num(v->u.as_list.item_count);
            if (v->kind == VAL_DICT) {
                int count = 0;
                for (int i = 0; i < TABLE_SIZE; i++) {
                    for (Entry *e = v->u.as_ent.fields->buckets[i]; e; e = e->next) {
                        count++;
                    }
                }
                return val_num(count);
            }
            return val_num(0);
        }
        case ND_ORD: {
            Value *v = eval(n->left, env);
            if (v->kind == VAL_STR && v->u.as_str[0]) return val_num((unsigned char)v->u.as_str[0]);
            return val_num(0);
        }
        case ND_CHR: {
            Value *v = eval(n->left, env);
            char buf[2] = { (char)(int)v->u.as_num, '\0' };
            return val_str(buf);
        }
        case ND_TONUM: {
            Value *v = eval(n->left, env);
            if (v->kind == VAL_STR) {
                char *end;
                double d = strtod(v->u.as_str, &end);
                if (*end == '\0' && end != v->u.as_str) return val_num(d);
                return val_num(0);
            }
            if (v->kind == VAL_NUM) return v;
            return val_num(0);
        }
        case ND_TOSTR: {
            Value *v = eval(n->left, env);
            if (v->kind == VAL_NUM) {
                char buf[64];
                if (v->u.as_num == (long long)v->u.as_num)
                    snprintf(buf, sizeof(buf), "%lld", (long long)v->u.as_num);
                else
                    snprintf(buf, sizeof(buf), "%g", v->u.as_num);
                return val_str(buf);
            }
            if (v->kind == VAL_STR) return v;
            return val_str("");
        }
        case ND_SLICE: {
            Value *v = eval(n->left, env);
            Value *sv = eval(n->body[0], env);
            Value *ev = eval(n->body[1], env);
            int s = (int)sv->u.as_num, e = (int)ev->u.as_num;
            if (v->kind == VAL_STR) {
                int l = strlen(v->u.as_str);
                if (s < 0) s += l;
                if (e < 0) e += l;
                if (s < 0) s = 0;
                if (e > l) e = l;
                if (s >= e) return val_str("");
                int len = e - s;
                char *b = arena_alloc(global_arena, len + 1);
                memcpy(b, v->u.as_str + s, len);
                b[len] = 0;
                Value *res = val_num(0); res->kind = VAL_STR; res->u.as_str = b;
                return res;
            }
            if (v->kind == VAL_LIST) {
                int l = v->u.as_list.item_count;
                if (s < 0) s += l;
                if (e < 0) e += l;
                if (s < 0) s = 0;
                if (e > l) e = l;
                Value *res = val_list();
                if (s >= e) return res;
                for (int i = s; i < e; i++) {
                    if (res->u.as_list.item_count >= res->u.as_list.item_cap) {
                        val_list_grow(res);
                    }
                    res->u.as_list.items[res->u.as_list.item_count++] = v->u.as_list.items[i];
                }
                return res;
            }
            return val_none();
        }
        case ND_TYPE: {
            Value *v = val_unwrap(eval(n->left, env));
            switch (v->kind) {
                case VAL_NUM:  return val_str("num");
                case VAL_STR:  return val_str("str");
                case VAL_LIST: return val_str("list");
                case VAL_DICT: return val_str("dict");
                case VAL_BIT:  return val_str("bit");
                case VAL_PTR:  return val_str("ptr");
                case VAL_ENT:  return val_str(v->u.as_ent.type_name);
                case VAL_UNI:  return val_str(v->u.as_ent.type_name);
                default:       return val_str("nada");
            }
        }
        case ND_TIME: return val_num((double)time(NULL));
        case ND_SYS: {
            Value *v = eval(n->left, env);
            if (v->kind != VAL_STR) return val_num(-1);
            int result = system(v->u.as_str);
            return val_num((double)result);
        }
        case ND_ENV: {
            Value *v = eval(n->left, env);
            if (v->kind != VAL_STR) return val_str("");
            char *e = getenv(v->u.as_str);
            return e ? val_str(e) : val_str("");
        }
        case ND_EXIT: exit((int)eval(n->left, env)->u.as_num); return val_none();
        case ND_CALL: {
            const char *fname = n->left->name;
            /* Handle built-in runtime functions */
            if (strcmp(fname, "arena_create") == 0) {
                if (n->body_count < 1) runtime_error(n, "arena_create requires a size argument");
                Value *size_val = eval(n->body[0], env);
                if (size_val->kind != VAL_NUM) runtime_error(n, "arena_create requires a numeric size");
                /* Arena is global, just return a dummy non-none value */
                return val_num(1);
            }
            if (strcmp(fname, "table_new") == 0) {
                Value *v = arena_alloc(global_arena, sizeof(Value));
                v->kind = VAL_DICT;
                v->u.as_ent.fields = table_new(NULL);
                return v;
            }
            if (strcmp(fname, "len") == 0) {
                if (n->body_count < 1) runtime_error(n, "len requires an argument");
                Value *v = val_unwrap(eval(n->body[0], env));
                if (v->kind == VAL_LIST) return val_num(v->u.as_list.item_count);
                if (v->kind == VAL_STR) return val_num(strlen(v->u.as_str));
                if (v->kind == VAL_DICT) {
                    int count = 0;
                    for (int i = 0; i < TABLE_SIZE; i++) {
                        for (Entry *e = v->u.as_ent.fields->buckets[i]; e; e = e->next) count++;
                    }
                    return val_num(count);
                }
                return val_num(0);
            }
            if (strcmp(fname, "slice") == 0) {
                if (n->body_count < 3) runtime_error(n, "slice requires string, start, end");
                Value *v = val_unwrap(eval(n->body[0], env));
                Value *start = val_unwrap(eval(n->body[1], env));
                Value *end = val_unwrap(eval(n->body[2], env));
                if (v->kind != VAL_STR || start->kind != VAL_NUM || end->kind != VAL_NUM) 
                    return val_str("");
                int s = (int)start->u.as_num;
                int e = (int)end->u.as_num;
                int len = strlen(v->u.as_str);
                if (s < 0) s = 0;
                if (e > len) e = len;
                if (s >= e) return val_str("");
                char *buf = malloc(e - s + 1);
                strncpy(buf, v->u.as_str + s, e - s);
                buf[e - s] = '\0';
                Value *r = val_str(buf);
                free(buf);
                return r;
            }
            if (strcmp(fname, "str_contains") == 0) {
                if (n->body_count < 2) runtime_error(n, "str_contains requires haystack, needle");
                Value *haystack = val_unwrap(eval(n->body[0], env));
                Value *needle = val_unwrap(eval(n->body[1], env));
                if (haystack->kind != VAL_STR || needle->kind != VAL_STR) return val_num(0);
                return val_num(strstr(haystack->u.as_str, needle->u.as_str) != NULL);
            }
            if (strcmp(fname, "str_replace") == 0) {
                if (n->body_count < 3) runtime_error(n, "str_replace requires string, target, replacement");
                Value *v = val_unwrap(eval(n->body[0], env));
                Value *t = val_unwrap(eval(n->body[1], env));
                Value *r = val_unwrap(eval(n->body[2], env));
                if (v->kind != VAL_STR || t->kind != VAL_STR || r->kind != VAL_STR) return val_str("");
                char *result = malloc(strlen(v->u.as_str) + 1);
                result[0] = '\0';
                char *p = v->u.as_str;
                size_t tl = strlen(t->u.as_str);
                size_t rl = strlen(r->u.as_str);
                while (*p) {
                    char *f = strstr(p, t->u.as_str);
                    if (f == p) {
                        strcat(result, r->u.as_str);
                        p += tl;
                    } else {
                        size_t len = strlen(result);
                        result[len] = *p++;
                        result[len+1] = '\0';
                    }
                }
                Value *rv = val_str(result);
                free(result);
                return rv;
            }
            if (strcmp(fname, "read_file") == 0) {
                if (n->body_count < 1) runtime_error(n, "read_file requires a filename");
                Value *v = val_unwrap(eval(n->body[0], env));
                if (v->kind != VAL_STR) return val_str("");
                FILE *f = fopen(v->u.as_str, "r");
                if (!f) return val_str("");
                fseek(f, 0, SEEK_END);
                long sz = ftell(f);
                rewind(f);
                char *buf = malloc(sz + 1);
                fread(buf, 1, sz, f);
                buf[sz] = '\0';
                fclose(f);
                Value *rv = val_str(buf);
                free(buf);
                return rv;
            }
            if (strcmp(fname, "arena_alloc") == 0) {
                if (n->body_count < 1) runtime_error(n, "arena_alloc requires a size");
                Value *size_val = val_unwrap(eval(n->body[0], env));
                if (size_val->kind != VAL_NUM) runtime_error(n, "arena_alloc requires a numeric size");
                void *p = arena_alloc(global_arena, (size_t)size_val->u.as_num);
                return val_num((long long)p);
            }
            if (strcmp(fname, "keys") == 0) {
                if (n->body_count < 1) runtime_error(n, "keys requires a dict argument");
                Value *v = val_unwrap(eval(n->body[0], env));
                if (v->kind != VAL_DICT) return val_none();
                // Count keys
                int count = 0;
                for (int i = 0; i < TABLE_SIZE; i++) {
                    for (Entry *e = v->u.as_ent.fields->buckets[i]; e; e = e->next) count++;
                }
                Value *list = arena_alloc(global_arena, sizeof(Value));
                list->kind = VAL_LIST;
                list->u.as_list.item_count = count;
                if (count > 0) {
                    list->u.as_list.items = arena_alloc(global_arena, count * sizeof(Value*));
                    int idx = 0;
                    for (int i = 0; i < TABLE_SIZE; i++) {
                        for (Entry *e = v->u.as_ent.fields->buckets[i]; e; e = e->next) {
                            list->u.as_list.items[idx++] = val_str(e->key);
                        }
                    }
                } else {
                    list->u.as_list.items = NULL;
                }
                return list;
            }
            Func *fn = func_find(fname);
            if (!fn) { runtime_error(n, "undefined function '%s'", fname); }
            int argc = n->body_count;
            Value **argv = malloc(argc * sizeof(Value*));
            for (int i = 0; i < argc; i++) {
                argv[i] = eval(n->body[i], env);
            }
            Table *scope = table_new(env);
            for (int i = 0; i < fn->def->body_count && i < argc; i++) {
                table_set(scope, fn->def->body[i]->name, argv[i]);
            }
            Value *result = eval_block(fn->def->left, scope);
            /* Ensure 'break' signal doesn't escape the function into the caller's loop */
            table_free(scope);
            free(argv);
            if (result->kind == VAL_BREAK) return val_none();
            if (result->kind == VAL_OUT) return result->u.as_inner;
            return result;
        }
        case ND_PRINT: {
            Value *v = eval(n->left, env);
            val_print(v);
            putchar('\n');
            fflush(stdout);
            return v;
        }
        case ND_BLOCK:
            return eval_block(n, env);
        case ND_NADA:
            return val_none();
        case ND_STACK_ALLOC:
            /* Stack allocation - create a fixed-size list for bounds checking */
            {
                Value *list = val_list();
                int size = (int)n->num;
                list->u.as_list.fixed_cap = size;  /* Mark as fixed array */
                if (size > 0) {
                    /* Pre-allocate to fixed size */
                    for (int i = 0; i < size; i++) {
                        if (i >= list->u.as_list.item_cap) val_list_grow(list);
                        list->u.as_list.items[i] = val_none();
                    }
                    list->u.as_list.item_count = size;
                }
                return list;
            }
        case ND_CAST: {
            /* Cast expression: value -> type */
            Value *val = eval(n->left, env);
            const char *target_type = n->type_tag;
            
            if (!target_type) {
                runtime_error(n, "cast missing target type");
            }
            
            /* For now, numeric casts just preserve the value but validate the type */
            // Handle pointer casts
            if (strstr(target_type, "ptr") == target_type) {
                return val; // For now, pointer casts just preserve the pointer value
            }
            /* Cast to void returns nada */
            if (strcmp(target_type, "void") == 0) {
                return val_none();
            }
            /* Future: implement actual bit-level conversions */
            if (strcmp(target_type, "i8") == 0 || strcmp(target_type, "i16") == 0 ||
                strcmp(target_type, "i32") == 0 || strcmp(target_type, "i64") == 0 ||
                strcmp(target_type, "u8") == 0 || strcmp(target_type, "u16") == 0 ||
                strcmp(target_type, "u32") == 0 || strcmp(target_type, "u64") == 0 ||
                strcmp(target_type, "f32") == 0 || strcmp(target_type, "f64") == 0) {
                /* Numeric cast - return as number for now */
                return val;
            } else if (strcmp(target_type, "str") == 0) {
                /* Cast to string */
                if (val->kind == VAL_NUM) {
                    char buf[64];
                    if (val->u.as_num == (double)(long long)val->u.as_num) {
                        snprintf(buf, sizeof(buf), "%lld", (long long)val->u.as_num);
                    } else {
                        snprintf(buf, sizeof(buf), "%g", val->u.as_num);
                    }
                    return val_str(buf);
                } else if (val->kind == VAL_STR) {
                    return val;
                } else {
                    runtime_error(n, "cannot cast %s to str", val_kind_name(val->kind));
                }
            } else if (strcmp(target_type, "num") == 0) {
                /* Cast to number */
                if (val->kind == VAL_NUM) {
                    return val;
                } else if (val->kind == VAL_STR) {
                    return val_num(atof(val->u.as_str));
                } else {
                    runtime_error(n, "cannot cast %s to num", val_kind_name(val->kind));
                }
            } else {
                runtime_error(n, "unknown cast target type: %s", target_type);
            }
        }
    }
    return val_none();
}

/* ──────────────────────── Debug ──────────────────────── */

void dump_ast_json(Node *n, FILE *f, int indent) {
    for (int i = 0; i < indent; i++) fprintf(f, "  ");
    fprintf(f, "{ ");
    fprintf(f, "\"kind\": \"%d\", ", n->kind);
    if (n->name) fprintf(f, "\"name\": \"%s\", ", n->name);
    if (n->type_tag) fprintf(f, "\"type\": \"%s\", ", n->type_tag);
    if (n->num != 0) fprintf(f, "\"num\": %g, ", n->num);
    if (n->str) fprintf(f, "\"str\": \"%s\", ", n->str);
    fprintf(f, "\"body_count\": %d", n->body_count);
    if (n->body_count > 0) {
        fprintf(f, ",\n");
        for (int i = 0; i < indent; i++) fprintf(f, "  ");
        fprintf(f, "  \"body\": [\n");
        for (int i = 0; i < n->body_count; i++) {
            dump_ast_json(n->body[i], f, indent + 2);
            if (i < n->body_count - 1) fprintf(f, ",\n");
        }
        fprintf(f, "\n");
        for (int i = 0; i < indent; i++) fprintf(f, "  ");
        fprintf(f, "  ]");
    }
    if (n->left) { fprintf(f, ",\n"); for (int i = 0; i < indent; i++) fprintf(f, "  "); fprintf(f, "  \"left\": \n"); dump_ast_json(n->left, f, indent + 2); }
    if (n->right) { fprintf(f, ",\n"); for (int i = 0; i < indent; i++) fprintf(f, "  "); fprintf(f, "  \"right\": \n"); dump_ast_json(n->right, f, indent + 2); }
    fprintf(f, " }");
}
