#include "vii.h"
#include <math.h>

/* ──────────────────────── Table ──────────────────────── */

static unsigned hash(const char *s) {
    unsigned h = 0;
    while (*s) h = h * 31 + (unsigned char)*s++;
    return h % TABLE_SIZE;
}

Table *table_new(Table *parent) {
    Table *t = calloc(1, sizeof(Table));
    t->parent = parent;
    return t;
}

Value *table_get(Table *t, const char *key) {
    for (Table *cur = t; cur; cur = cur->parent) {
        unsigned h = hash(key);
        for (Entry *e = cur->buckets[h]; e; e = e->next) {
            if (strcmp(e->key, key) == 0) return e->val;
        }
    }
    return NULL;
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
    Entry *e = malloc(sizeof(Entry));
    e->key = strdup(key);
    e->val = val;
    e->is_constant = is_all_caps(key);
    e->next = t->buckets[h];
    t->buckets[h] = e;
}

/* ──────────────────────── Function registry ──────────────────────── */

static Func *funcs = NULL;

static void func_register(const char *name, Node *def) {
    Func *f = malloc(sizeof(Func));
    f->name = strdup(name);
    f->def = def;
    f->next = funcs;
    funcs = f;
}

static Func *func_find(const char *name) {
    for (Func *f = funcs; f; f = f->next)
        if (strcmp(f->name, name) == 0) return f;
    return NULL;
}

/* ──────────────────────── CLI args ──────────────────────── */

Value *cli_args = NULL;

/* ──────────────────────── Eval ──────────────────────── */

Value *eval_block(Node *block, Table *env) {
    Value *last = val_none();
    for (int i = 0; i < block->body_count; i++) {
        last = eval(block->body[i], env);
    }
    return last;
}

Value *eval(Node *n, Table *env) {
    if (!n) return val_none();

    switch (n->kind) {
        case ND_NUM:   return val_num(n->num);
        case ND_STR:   return val_str(n->str);
        case ND_UMINUS: {
            Value *v = eval(n->left, env);
            return val_num(0 - v->num);
        }
        case ND_VAR:   {
            Value *v = table_get(env, n->name);
            if (!v) {
                Func *fn = func_find(n->name);
                if (fn) {
                    Table *scope = table_new(env);
                    return eval_block(fn->def->left, scope);
                }
                fprintf(stderr, "Runtime error: undefined variable '%s'\n", n->name); exit(1);
            }
            return v;
        }
        case ND_ASSIGN: {
            Value *val = eval(n->right, env);
            if (n->left->kind == ND_VAR) {
                table_set(env, n->left->name, val);
            } else if (n->left->kind == ND_AT) {
                Value *list = eval(n->left->left, env);
                Value *idx  = eval(n->left->right, env);
                if (list->kind != VAL_LIST) { fprintf(stderr, "Runtime error: 'at' on non-list\n"); exit(1); }
                int i = (int)idx->num;
                if (i < 0) i += list->item_count;
                if (i == list->item_count) {
                    if (list->item_count >= list->item_cap) {
                        list->item_cap = list->item_cap ? list->item_cap * 2 : 8;
                        list->items = realloc(list->items, list->item_cap * sizeof(Value*));
                    }
                    list->items[list->item_count++] = val;
                } else {
                    if (i < 0 || i >= list->item_count) { fprintf(stderr, "Runtime error: index %d out of range\n", i); exit(1); }
                    list->items[i] = val;
                }
            }
            return val;
        }
        case ND_BINOP: {
            /* short-circuit for and/or */
            if (n->op == TOK_AND) {
                Value *l = eval(n->left, env);
                if (!val_truthy(l)) return val_num(0);
                Value *r = eval(n->right, env);
                return val_num(val_truthy(r) ? 1 : 0);
            }
            if (n->op == TOK_OR) {
                Value *l = eval(n->left, env);
                if (val_truthy(l)) return val_num(1);
                Value *r = eval(n->right, env);
                return val_num(val_truthy(r) ? 1 : 0);
            }
            Value *l = eval(n->left, env);
            Value *r = eval(n->right, env);
            if (l->kind == VAL_STR && r->kind == VAL_STR) {
                int cmp = strcmp(l->str, r->str);
                switch (n->op) {
                    case TOK_LT:  return val_num(cmp < 0 ? 1 : 0);
                    case TOK_GT:  return val_num(cmp > 0 ? 1 : 0);
                    case TOK_LTE: return val_num(cmp <= 0 ? 1 : 0);
                    case TOK_GTE: return val_num(cmp >= 0 ? 1 : 0);
                    default: break;
                }
            }
            double a = l->num, b = r->num;
            if (n->op == TOK_PLUS && (l->kind == VAL_STR || r->kind == VAL_STR)) {
                char buf[1024];
                char la[512], ra[512];
                if (l->kind == VAL_STR) snprintf(la, sizeof(la), "%s", l->str);
                else snprintf(la, sizeof(la), "%g", l->num);
                if (r->kind == VAL_STR) snprintf(ra, sizeof(ra), "%s", r->str);
                else snprintf(ra, sizeof(ra), "%g", r->num);
                snprintf(buf, sizeof(buf), "%s%s", la, ra);
                return val_str(buf);
            }
            switch (n->op) {
                case TOK_PLUS:  return val_num(a + b);
                case TOK_MINUS: return val_num(a - b);
                case TOK_STAR:  return val_num(a * b);
                case TOK_SLASH: return val_num(b != 0 ? a / b : 0);
                case TOK_PCT:   return val_num(b != 0 ? fmod(a, b) : 0);
                case TOK_LT:    return val_num(a < b ? 1 : 0);
                case TOK_GT:    return val_num(a > b ? 1 : 0);
                case TOK_LTE:   return val_num(a <= b ? 1 : 0);
                case TOK_GTE:   return val_num(a >= b ? 1 : 0);
                case TOK_NE:    {
                    if (l->kind == VAL_STR && r->kind == VAL_STR)
                        return val_num(strcmp(l->str, r->str) != 0 ? 1 : 0);
                    return val_num(a != b ? 1 : 0);
                }
                case TOK_EQEQ:  {
                    if (l->kind == VAL_STR && r->kind == VAL_STR)
                        return val_num(strcmp(l->str, r->str) == 0 ? 1 : 0);
                    return val_num(a == b ? 1 : 0);
                }
                default: return val_num(0);
            }
        }
        case ND_IF: {
            Value *cond = eval(n->left, env);
            Value *result = val_none();
            if (val_truthy(cond)) {
                if (n->body_count > 0) {
                    for (int i = 0; i < n->body_count; i++)
                        result = eval(n->body[i], env);
                }
            } else if (n->right) {
                if (n->right->kind == ND_BLOCK) {
                    result = eval_block(n->right, env);
                } else {
                    result = eval(n->right, env);
                }
            }
            return result;
        }
        case ND_WHILE: {
            while (val_truthy(eval(n->left, env))) {
                for (int i = 0; i < n->body_count; i++)
                    eval(n->body[i], env);
            }
            return val_none();
        }
        case ND_DO: {
            func_register(n->name, n);
            return val_none();
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
            FILE *f = fopen(path->str, "rb");
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
        case ND_AT: {
            Value *list = eval(n->left, env);
            Value *idx  = eval(n->right, env);
            if (list->kind == VAL_STR) {
                int i = (int)idx->num;
                if (i < 0) i += (int)strlen(list->str);
                if (i < 0 || i >= (int)strlen(list->str)) return val_none();
                char buf[2] = { list->str[i], '\0' };
                return val_str(buf);
            }
            if (list->kind != VAL_LIST) { fprintf(stderr, "Runtime error: 'at' on non-list\n"); exit(1); }
            int i = (int)idx->num;
            if (i < 0) i += list->item_count;
            if (i < 0 || i >= list->item_count) return val_none();
            return list->items[i];
        }
        case ND_SET: {
            Value *list = eval(n->left, env);
            if (list->kind != VAL_LIST) { fprintf(stderr, "Runtime error: 'set' on non-list\n"); exit(1); }
            Value *idx  = eval(n->body[0], env);
            Value *val  = eval(n->body[1], env);
            int i = (int)idx->num;
            if (i < 0) i += list->item_count;
            if (i == list->item_count) {
                if (list->item_count >= list->item_cap) {
                    list->item_cap = list->item_cap ? list->item_cap * 2 : 8;
                    list->items = realloc(list->items, list->item_cap * sizeof(Value*));
                }
                list->items[list->item_count++] = val;
            } else {
                if (i < 0 || i >= list->item_count) { fprintf(stderr, "Runtime error: index %d out of range\n", i); exit(1); }
                list->items[i] = val;
            }
            return val;
        }
        case ND_PUT: {
            Value *path = eval(n->left, env);
            Value *data = eval(n->right, env);
            if (path->kind != VAL_STR) { fprintf(stderr, "Runtime error: put requires a string path\n"); exit(1); }
            FILE *f = fopen(path->str, "w");
            if (!f) { fprintf(stderr, "Runtime error: cannot open '%s' for writing\n", path->str); exit(1); }
            if (data->kind == VAL_STR) fprintf(f, "%s", data->str);
            else val_print_to(data, f);
            fclose(f);
            return val_none();
        }
        case ND_ARG: {
            return cli_args;
        }
        case ND_LEN: {
            Value *v = eval(n->left, env);
            if (v->kind == VAL_STR) return val_num((double)strlen(v->str));
            if (v->kind == VAL_LIST) return val_num(v->item_count);
            return val_num(0);
        }
        case ND_ORD: {
            Value *v = eval(n->left, env);
            if (v->kind == VAL_STR && v->str[0]) return val_num((unsigned char)v->str[0]);
            return val_num(0);
        }
        case ND_CHR: {
            Value *v = eval(n->left, env);
            char buf[2] = { (char)(int)v->num, '\0' };
            return val_str(buf);
        }
        case ND_TONUM: {
            Value *v = eval(n->left, env);
            if (v->kind == VAL_STR) {
                char *end;
                double d = strtod(v->str, &end);
                if (*end == '\0' && end != v->str) return val_num(d);
                return val_num(0);
            }
            if (v->kind == VAL_NUM) return v;
            return val_num(0);
        }
        case ND_TOSTR: {
            Value *v = eval(n->left, env);
            if (v->kind == VAL_NUM) {
                char buf[64];
                if (v->num == (long long)v->num)
                    snprintf(buf, sizeof(buf), "%lld", (long long)v->num);
                else
                    snprintf(buf, sizeof(buf), "%g", v->num);
                return val_str(buf);
            }
            if (v->kind == VAL_STR) return v;
            return val_str("");
        }
        case ND_SYS: {
            Value *v = eval(n->left, env);
            if (v->kind != VAL_STR) return val_num(-1);
            int result = system(v->str);
            return val_num((double)result);
        }
        case ND_ENV: {
            Value *v = eval(n->left, env);
            if (v->kind != VAL_STR) return val_str("");
            char *e = getenv(v->str);
            return e ? val_str(e) : val_str("");
        }
        case ND_EXIT: exit((int)eval(n->left, env)->num); return val_none();
        case ND_CALL: {
            const char *fname = n->left->name;
            Func *fn = func_find(fname);
            if (!fn) { fprintf(stderr, "Runtime error: undefined function '%s'\n", fname); exit(1); }
            int argc = n->body_count;
            Value **argv = malloc(argc * sizeof(Value*));
            for (int i = 0; i < argc; i++)
                argv[i] = eval(n->body[i], env);
            Table *scope = table_new(env);
            for (int i = 0; i < fn->def->body_count && i < argc; i++) {
                table_set(scope, fn->def->body[i]->name, argv[i]);
            }
            Value *result = eval_block(fn->def->left, scope);
            free(argv);
            return result;
        }
        case ND_PRINT: {
            Value *v = eval(n->left, env);
            val_print(v);
            putchar('\n');
            return v;
        }
        case ND_BLOCK:
            return eval_block(n, env);
    }
    return val_none();
}
