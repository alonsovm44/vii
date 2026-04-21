#include "vii.h"

/* ──────────────────────── Debug Dumper ──────────────────────── */

void dump_ast_json(Node *n, FILE *f, int indent) {
    for (int i = 0; i < indent; i++) fprintf(f, "  ");
    fprintf(f, "{ \"kind\": %d", n->kind);
    if (n->name) fprintf(f, ", \"name\": \"%s\"", n->name);
    if (n->kind == ND_NUM) fprintf(f, ", \"val\": %g", n->num);
    if (n->str) fprintf(f, ", \"str\": \"%s\"", n->str);
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

/* ──────────────────────── Codegen (IO -> C) ──────────────────────── */

static void emit_c_header(FILE *f) {
    fprintf(f, "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <stdbool.h>\n#include <math.h>\n#include <time.h>\n\n");
    fprintf(f, "typedef enum { VAL_NUM, VAL_STR, VAL_LIST, VAL_DICT, VAL_FUNC, VAL_BIT, VAL_REF, VAL_NONE } ValKind;\n");
    fprintf(f, "struct Table;\nstruct Value;\n");
    fprintf(f, "typedef struct Value* (*IoFunc)(struct Table*, int, struct Value**);\n");
    fprintf(f, "typedef struct Value { ValKind kind; double num; char *str; struct Value **items; int item_count; int item_cap; struct Table *fields; IoFunc func; struct Value *target; } Value;\n");
    fprintf(f, "typedef struct Entry { char *key; Value *val; bool is_constant; struct Entry *next; } Entry;\n");
    fprintf(f, "typedef struct Table { Entry *buckets[256]; struct Table *parent; } Table;\n\n");

    /* Allocation Helpers */
    fprintf(f, "static Value *val_num(double n) { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_NUM; v->num = n; return v; }\n");
    fprintf(f, "static Value *val_str(const char *s) { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_STR; v->str = strdup(s); return v; }\n");
    fprintf(f, "static Value *val_list() { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_LIST; v->item_cap = 8; v->items = calloc(8, sizeof(Value*)); return v; }\n");
    fprintf(f, "static Value *val_dict() { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_DICT; v->fields = table_new(NULL); return v; }\n");
    fprintf(f, "static Value *val_func(IoFunc f) { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_FUNC; v->func = f; return v; }\n");
    fprintf(f, "static Value *val_bit(bool b) { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_BIT; v->num = b ? 1 : 0; return v; }\n");
    fprintf(f, "static Value *val_ref(Value *t) { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_REF; v->target = t; return v; }\n");
    fprintf(f, "static Value *val_none() { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_NONE; return v; }\n\n");

    /* Runtime Table Logic */
    fprintf(f, "static unsigned hash(const char *s) { unsigned h = 0; while(*s) h = h * 31 + (unsigned char)*s++; return h %% 256; }\n");
    fprintf(f, "static Table* table_new(Table *p) { Table *t = calloc(1, sizeof(Table)); t->parent = p; return t; }\n");
    fprintf(f, "static Value* table_get(Table *t, const char *k) { for(Table *cur=t;cur;cur=cur->parent){ unsigned h=hash(k); for(Entry *e=cur->buckets[h];e;e=e->next) if(!strcmp(e->key,k)) return e->val; } return val_none(); }\n");
    fprintf(f, "static bool is_all_caps(const char *s) { if(!s||!*s)return false; bool h=false; for(const char *c=s;*c;c++){ if(*c>='a'&&*c<='z') return false; if(*c>='A'&&*c<='Z') h=true; } return h; }\n");
    fprintf(f, "static void table_set(Table *t, const char *k, Value *v) { unsigned h = hash(k); for(Entry *e=t->buckets[h];e;e=e->next) if(!strcmp(e->key,k)){ if(e->is_constant){fprintf(stderr,\"Runtime error: cannot reassign constant '%%s'\\n\",k);exit(1);} if(e->val->kind == VAL_REF) { Value *trg = e->val->target; trg->kind=v->kind; trg->num=v->num; trg->str=v->str; trg->items=v->items; trg->item_count=v->item_count; trg->fields=v->fields; return; } e->val=v;return;} Entry *e=malloc(sizeof(Entry)); e->key=strdup(k); e->val=v; e->is_constant=is_all_caps(k); e->next=t->buckets[h]; t->buckets[h]=e; }\n\n");

    /* Builtin Operations */
    fprintf(f, "static Value* val_unwrap(Value* v) { while(v && v->kind == VAL_REF) v = v->target; return v; }\n");
    fprintf(f, "static bool val_truthy(Value *v) { v = val_unwrap(v); if(!v) return false; if(v->kind==VAL_NUM) return v->num != 0; if(v->kind==VAL_STR) return v->str[0]!='\\0'; return v->item_count > 0; }\n");
    fprintf(f, "static void val_print(Value *v) { v = val_unwrap(v); if(!v) return; if(v->kind==VAL_NUM) printf(v->num==(long long)v->num?\"%%lld\":\"%%g\",(long long)v->num); else if(v->kind==VAL_STR) printf(\"%%s\",v->str); else if(v->kind==VAL_LIST){ printf(\"[\"); for(int i=0;i<v->item_count;i++){ if(i)printf(\", \"); val_print(v->items[i]); } printf(\"]\"); } }\n");
    fprintf(f, "static Value* runtime_binop(int op, Value* l, Value* r) {\n");
    fprintf(f, "  l = val_unwrap(l); r = val_unwrap(r);\n");
    fprintf(f, "  if(op==%d && (l->kind==VAL_STR||r->kind==VAL_STR)){ char b[2048]; sprintf(b, \"%%s%%s\", l->kind==VAL_STR?l->str:\"\", r->kind==VAL_STR?r->str:\"\"); return val_str(b); }\n", TOK_PLUS);
    fprintf(f, "  if(l->kind==VAL_STR && r->kind==VAL_STR) {\n");
    fprintf(f, "    int c=strcmp(l->str,r->str); switch(op){\n");
    fprintf(f, "      case %d: return val_bit(c==0); case %d: return val_bit(c!=0);\n", TOK_EQEQ, TOK_NE);
    fprintf(f, "      case %d: return val_bit(c<0); case %d: return val_bit(c>0); case %d: return val_bit(c<=0); case %d: return val_bit(c>=0);\n", TOK_LT, TOK_GT, TOK_LTE, TOK_GTE);
    fprintf(f, "    }\n  }\n");
    fprintf(f, "  double a=l->num, b=r->num; switch(op){\n");
    fprintf(f, "    case %d: return val_num(a+b); case %d: return val_num(a-b); case %d: return val_num(a*b); case %d: return val_num(b?a/b:0); case %d: return val_num(b?fmod(a,b):0);\n", TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PCT);
    fprintf(f, "    case %d: return val_bit(a<b); case %d: return val_bit(a>b); case %d: return val_bit(a<=b); case %d: return val_bit(a>=b); case %d: return val_bit(a!=b); case %d: return val_bit(a==b); case %d: return val_bit(val_truthy(l)&&val_truthy(r)); case %d: return val_bit(val_truthy(l)||val_truthy(r)); default: return val_none();\n", TOK_LT, TOK_GT, TOK_LTE, TOK_GTE, TOK_NE, TOK_EQEQ, TOK_AND, TOK_OR);
    fprintf(f, "  }\n}\n");
    fprintf(f, "static Value* runtime_call(Value* f, Table* e, int c, Value** v) { if(f->kind==VAL_FUNC) return f->func(e, c, v); return val_none(); }\n");
    fprintf(f, "static Value* io_args;\n");
    fprintf(f, "static Value* runtime_var_get(Table* e, const char* k) { Value* v=table_get(e,k); if(v->kind==VAL_FUNC) return v->func(e,0,NULL); return v; }\n");
    fprintf(f, "static Value* runtime_at(Value* l, Value* r) { l = val_unwrap(l); r = val_unwrap(r); int i=(int)r->num; if(l->kind==VAL_STR){ if(i<0||i>=strlen(l->str)) return val_none(); char b[2]={l->str[i],0}; return val_str(b); } if(l->kind==VAL_LIST){ if(i<0||i>=l->item_count) return val_none(); return l->items[i]; } return val_none(); }\n");
    fprintf(f, "static void runtime_set(Value* l, Value* i, Value* v) { l = val_unwrap(l); i = val_unwrap(i); if(l->kind!=VAL_LIST) return; int idx=(int)i->num; if(idx==l->item_count){ if(l->item_count>=l->item_cap){ l->item_cap*=2; l->items=realloc(l->items,l->item_cap*sizeof(Value*)); } l->items[l->item_count++]=v; } else if(idx>=0 && idx<l->item_count) l->items[idx]=v; }\n");
    fprintf(f, "static Value* runtime_ask(Value* p) { p = val_unwrap(p); if(p&&p->kind==VAL_STR){ FILE* f=fopen(p->str,\"rb\"); if(!f) return val_str(\"\"); fseek(f,0,SEEK_END); long l=ftell(f); fseek(f,0,SEEK_SET); char* b=malloc(l+1); fread(b,1,l,f); b[l]=0; fclose(f); Value* v=val_str(b); free(b); return v; } char buf[1024]; if(!fgets(buf,1024,stdin)) return val_str(\"\"); buf[strcspn(buf,\"\\n\")]=0; return val_str(buf); }\n");
    fprintf(f, "static Value* runtime_len(Value* v) { v = val_unwrap(v); if(v->kind==VAL_STR) return val_num(strlen(v->str)); if(v->kind==VAL_LIST) return val_num(v->item_count); if(v->kind==VAL_DICT){ int c=0; for(int i=0;i<256;i++) for(Entry *e=v->fields->buckets[i];e;e=e->next) c++; return val_num(c); } return val_num(0); }\n");
    fprintf(f, "static Value* runtime_ord(Value* v) { v = val_unwrap(v); if(v->kind==VAL_STR&&v->str[0]) return val_num((unsigned char)v->str[0]); return val_num(0); }\n");
    fprintf(f, "static Value* runtime_chr(Value* v) { v = val_unwrap(v); char b[2]={v->num,0}; return val_str(b); }\n");
    fprintf(f, "static Value* runtime_tonum(Value* v) { v = val_unwrap(v); if(v->kind==VAL_STR){ char*e; double d=strtod(v->str,&e); return val_num(*e==0&&e!=v->str?d:0); } if(v->kind==VAL_NUM) return v; return val_num(0); }\n");
    fprintf(f, "static Value* runtime_str_concat(const char* a, const char* b) { char buf[2048]; snprintf(buf, 2048, \"%%s%%s\", a, b); return val_str(buf); }\n");
    fprintf(f, "static Value* runtime_put(Value* p, Value* d, int a) { p = val_unwrap(p); d = val_unwrap(d); if(p->kind!=VAL_STR) return val_none(); FILE* f=fopen(p->str, a?\"a\":\"w\"); if(!f) return val_none(); if(d->kind==VAL_STR) fprintf(f,\"%%s\",d->str); else { if(d->kind==VAL_NUM) fprintf(f,d->num==(long long)d->num?\"%%lld\":\"%%g\",(long long)d->num); } fclose(f); return val_none(); }\n");
    fprintf(f, "static Value* runtime_tostr(Value* v) { v = val_unwrap(v); char b[64]; if(v->kind==VAL_NUM){ if(v->num==(long long)v->num) snprintf(b,64,\"%%%%lld\",(long long)v->num); else snprintf(b,64,\"%%%%g\",v->num); return val_str(b); } if(v->kind==VAL_STR) return v; return val_str(\"\"); }\n");
    fprintf(f, "static Value* runtime_slice(Value* v, Value* sv, Value* ev) { v = val_unwrap(v); sv = val_unwrap(sv); ev = val_unwrap(ev); int s=(int)sv->num, e=(int)ev->num; if(v->kind==VAL_STR){ int l=strlen(v->str); if(s<0)s+=l; if(e<0)e+=l; if(s<0)s=0; if(e>l)e=l; if(s>=e)return val_str(\"\"); int n=e-s; char *b=malloc(n+1); memcpy(b,v->str+s,n); b[n]=0; Value *res=val_str(b); free(b); return res; } if(v->kind==VAL_LIST){ int l=v->item_count; if(s<0)s+=l; if(e<0)e+=l; if(s<0)s=0; if(e>l)e=l; Value *res=val_list(); if(s>=e)return res; for(int i=s;i<e;i++){ if(res->item_count>=res->item_cap){ res->item_cap*=2; res->items=realloc(res->items,res->item_cap*sizeof(Value*)); } res->items[res->item_count++]=v->items[i]; } return res; } return val_none(); }\n");
    fprintf(f, "static Value* runtime_type(Value* v) { v = val_unwrap(v); switch(v->kind){ case VAL_NUM: return val_str(\"num\"); case VAL_STR: return val_str(\"str\"); case VAL_LIST: return val_str(\"list\"); case VAL_DICT: return val_str(\"dict\"); case VAL_BIT: return val_str(\"bit\"); default: return val_str(\"none\"); } }\n");
    fprintf(f, "static Value* runtime_sys(Value* v) { v = val_unwrap(v); if(v->kind!=VAL_STR) return val_num(-1); return val_num((double)system(v->str)); }\n");
    fprintf(f, "static Value* runtime_env(Value* v) { v = val_unwrap(v); if(v->kind!=VAL_STR) return val_str(\"\"); char* e=getenv(v->str); return e?val_str(e):val_str(\"\"); }\n");
}

static void emit_c_expr(Node *n, FILE *f);
static void emit_c_stmt(Node *n, int indent, FILE *f);

static void emit_c_expr(Node *n, FILE *f) {
    bool is_num_type = (n->type_tag && !strcmp(n->type_tag, "num"));
    bool is_str_type = (n->type_tag && !strcmp(n->type_tag, "str"));
    switch (n->kind) {
        case ND_NUM:   fprintf(f, "val_num(%g)", n->num); break;
        case ND_UMINUS: fprintf(f, "val_num(0 - ("); emit_c_expr(n->left, f); fprintf(f, ")->num)"); break;
        case ND_STR: {
            fprintf(f, "val_str(\"");
            for (char *p = n->str; *p; p++) {
                if (*p == '\n') fprintf(f, "\\n");
                else if (*p == '\r') fprintf(f, "\\r");
                else if (*p == '\t') fprintf(f, "\\t");
                else if (*p == '\\') fprintf(f, "\\\\");
                else if (*p == '\"') fprintf(f, "\\\"");
                else fputc(*p, f);
            }
            fprintf(f, "\")");
            break;
        }
        case ND_VAR:
            if (is_num_type || is_str_type) fprintf(f, "%s", n->name);
            else fprintf(f, "runtime_var_get(env, \"%s\")", n->name);
            break;
        case ND_ARG:   fprintf(f, "io_args"); break;
        case ND_BINOP:
            if ((n->left->type_tag && !strcmp(n->left->type_tag, "num")) || n->left->kind == ND_NUM) {
                fprintf(f, "val_num(");
                if (n->left->kind == ND_NUM) fprintf(f, "%g", n->left->num);
                else emit_c_expr(n->left, f);

                if (n->op == TOK_PLUS) fprintf(f, " + ");
                else if (n->op == TOK_MINUS) fprintf(f, " - ");
                else if (n->op == TOK_STAR) fprintf(f, " * ");
                else if (n->op == TOK_SLASH) fprintf(f, " / ");

                if (n->right->kind == ND_NUM) fprintf(f, "%g", n->right->num);
                else emit_c_expr(n->right, f);
                fprintf(f, ")");
            } else if (n->op == TOK_PLUS && ((n->left->type_tag && !strcmp(n->left->type_tag, "str")) || n->left->kind == ND_STR)) {
                fprintf(f, "runtime_str_concat(");
                if (n->left->kind == ND_STR) fprintf(f, "\"%s\"", n->left->str);
                else emit_c_expr(n->left, f);
                fprintf(f, ", ");
                if (n->right->kind == ND_STR) fprintf(f, "\"%s\"", n->right->str);
                else if (n->right->type_tag && !strcmp(n->right->type_tag, "str")) emit_c_expr(n->right, f);
                else { fprintf(f, "("); emit_c_expr(n->right, f); fprintf(f, ")->str"); }
                fprintf(f, ")");
            } else {
                fprintf(f, "runtime_binop(%d, ", n->op);
                emit_c_expr(n->left, f);
                fprintf(f, ", ");
                emit_c_expr(n->right, f);
                fprintf(f, ")");
            }
            break;
        case ND_CALL:
            fprintf(f, "runtime_call(table_get(env, \"%s\"), env, %d, (Value*[]){", n->left->name, n->body_count);
            for (int i = 0; i < n->body_count; i++) {
                if (i > 0) fprintf(f, ", ");
                emit_c_expr(n->body[i], f);
            }
            fprintf(f, "})");
            break;
        case ND_ASSIGN:
            if (n->left->kind == ND_VAR) {
                if (n->left->type_tag && !strcmp(n->left->type_tag, "num")) {
                    fprintf(f, "%s = ", n->left->name);
                    emit_c_expr(n->right, f);
                } else if (n->left->type_tag && !strcmp(n->left->type_tag, "str")) {
                    fprintf(f, "%s = (", n->left->name);
                    emit_c_expr(n->right, f);
                    fprintf(f, ")->str");
                } else {
                    fprintf(f, "table_set(env, \"%s\", ", n->left->name);
                    emit_c_expr(n->right, f);
                    fprintf(f, ")");
                }
            } else if (n->left->kind == ND_AT) {
                fprintf(f, "runtime_set(");
                emit_c_expr(n->left->left, f);
                fprintf(f, ", ");
                emit_c_expr(n->left->right, f);
                fprintf(f, ", ");
                emit_c_expr(n->right, f);
                fprintf(f, ")");
            }
            break;
        case ND_LIST:  fprintf(f, "val_list()"); break;
        case ND_ASK:   fprintf(f, "runtime_ask(NULL)"); break;
        case ND_ASKFILE:
            fprintf(f, "runtime_ask(");
            emit_c_expr(n->left, f);
            fprintf(f, ")");
            break;
        case ND_AT:
            fprintf(f, "runtime_at(");
            emit_c_expr(n->left, f);
            fprintf(f, ", ");
            emit_c_expr(n->right, f);
            fprintf(f, ")");
            break;
        case ND_SET:
            fprintf(f, "runtime_set(");
            emit_c_expr(n->left, f);
            fprintf(f, ", ");
            emit_c_expr(n->body[0], f);
            fprintf(f, ", ");
            emit_c_expr(n->body[1], f);
            fprintf(f, ")");
            break;
        case ND_PUT:
            fprintf(f, "runtime_put(");
            emit_c_expr(n->left, f);
            fprintf(f, ", ");
            emit_c_expr(n->right, f);
            fprintf(f, ")");
            break;
        case ND_LEN:
            fprintf(f, "runtime_len(");
            emit_c_expr(n->left, f);
            fprintf(f, ")");
            break;
        case ND_ORD:
            fprintf(f, "runtime_ord(");
            emit_c_expr(n->left, f);
            fprintf(f, ")");
            break;
        case ND_CHR:
            fprintf(f, "runtime_chr(");
            emit_c_expr(n->left, f);
            fprintf(f, ")");
            break;
        case ND_TONUM:
            fprintf(f, "runtime_tonum(");
            emit_c_expr(n->left, f);
            fprintf(f, ")");
            break;
        case ND_TOSTR:
            fprintf(f, "runtime_tostr(");
            emit_c_expr(n->left, f);
            fprintf(f, ")");
            break;
        case ND_SLICE:
            fprintf(f, "runtime_slice(");
            emit_c_expr(n->left, f);
            fprintf(f, ", ");
            emit_c_expr(n->body[0], f);
            fprintf(f, ", ");
            emit_c_expr(n->body[1], f);
            fprintf(f, ")");
            break;
        case ND_TYPE: fprintf(f, "runtime_type("); emit_c_expr(n->left, f); fprintf(f, ")"); break;
        case ND_TIME: fprintf(f, "val_num((double)time(NULL))"); break;
        case ND_SYS:
            fprintf(f, "runtime_sys(");
            emit_c_expr(n->left, f);
            fprintf(f, ")");
            break;
        case ND_REF:
            fprintf(f, "val_ref(table_get(env, \"%s\"))", n->left->name);
            break;
        case ND_ENV:
            fprintf(f, "runtime_env(");
            emit_c_expr(n->left, f);
            fprintf(f, ")");
            break;
        case ND_EXIT:
            fprintf(f, "exit((int)("); emit_c_expr(n->left, f); fprintf(f, ")->num)");
            break;
        default: fprintf(f, "val_none()"); break;
    }
}

static void emit_c_stmt(Node *n, int indent, FILE *f) {
    for (int i = 0; i < indent; i++) fprintf(f, "  ");
    switch (n->kind) {
        case ND_ASSIGN:
            if (n->left->type_tag && !strcmp(n->left->type_tag, "num"))
                fprintf(f, "double ");
            else if (n->left->type_tag && !strcmp(n->left->type_tag, "str"))
                fprintf(f, "char* ");
            emit_c_expr(n, f);
            fprintf(f, ";\n");
            break;
        case ND_BLOCK:
            for (int i = 0; i < n->body_count; i++) emit_c_stmt(n->body[i], 0, f);
            break;
        case ND_IF:
            fprintf(f, "if (val_truthy(");
            emit_c_expr(n->left, f);
            fprintf(f, ")) {\n");
            emit_c_stmt(n->body[0], indent + 1, f);
            for (int i = 0; i < indent; i++) fprintf(f, "  ");
            fprintf(f, "}");
            if (n->right) {
                fprintf(f, " else {\n");
                emit_c_stmt(n->right, indent + 1, f);
                for (int i = 0; i < indent; i++) fprintf(f, "  ");
                fprintf(f, "}");
            }
            fprintf(f, "\n");
            break;
        case ND_WHILE:
            fprintf(f, "while (val_truthy(");
            emit_c_expr(n->left, f);
            fprintf(f, ")) {\n");
            emit_c_stmt(n->body[0], indent + 1, f);
            for (int i = 0; i < indent; i++) fprintf(f, "  ");
            fprintf(f, "}\n");
            break;
        case ND_PRINT:
            fprintf(f, "val_print(");
            emit_c_expr(n->left, f);
            fprintf(f, "); printf(\"\\n\");\n");
            break;
        case ND_DO:
            fprintf(f, "table_set(env, \"%s\", val_func(io_func_%s));\n", n->name, n->name);
            break;
        default:
            emit_c_expr(n, f);
            fprintf(f, ";\n");
            break;
    }
}

static void emit_c_functions(Node *n, FILE *f) {
    if (!n) return;
    if (n->kind == ND_BLOCK) {
        for (int i = 0; i < n->body_count; i++) emit_c_functions(n->body[i], f);
    } else if (n->kind == ND_DO) {
        /* Use the return type hint to generate more specific C return types if desired,
           though for the IoFunc pointer compatibility we usually stay with Value* */
        fprintf(f, "static Value* io_func_%s(Table* parent, int argc, Value** argv) {\n", n->name);
        fprintf(f, "  Table* env = table_new(parent);\n");

        /* If do->task was used, we could log it or register it here */
        if (n->mod_tag) fprintf(f, "  /* Modifier: %s */\n", n->mod_tag);

        for (int i = 0; i < n->body_count; i++) {
            if (n->body[i]->type_tag && !strcmp(n->body[i]->type_tag, "num")) {
                fprintf(f, "  double %s = (argc > %d && argv[%d]->kind == VAL_NUM) ? argv[%d]->num : 0;\n",
                        n->body[i]->name, i, i, i);
            } else if (n->body[i]->type_tag && !strcmp(n->body[i]->type_tag, "str")) {
                fprintf(f, "  char* %s = (argc > %d && argv[%d]->kind == VAL_STR) ? argv[%d]->str : \"\";\n",
                        n->body[i]->name, i, i, i);
            } else {
                fprintf(f, "  if(argc > %d) table_set(env, \"%s\", argv[%d]);\n", i, n->body[i]->name, i);
            }
        }
        emit_c_functions(n->left, f);
        Node *block = n->left;
        for (int i = 0; i < block->body_count; i++) {
            if (i == block->body_count - 1 && block->body[i]->kind != ND_IF && block->body[i]->kind != ND_WHILE && block->body[i]->kind != ND_EXIT && block->body[i]->kind != ND_BLOCK) {
                fprintf(f, "  return "); emit_c_expr(block->body[i], f); fprintf(f, ";\n");
            } else {
                emit_c_stmt(block->body[i], 1, f);
            }
        }
        fprintf(f, "  return val_none();\n}\n\n");
    }
}

void compile_to_bin(Node *prog, const char *out_name, bool keep_c) {
    char c_file[512];
    snprintf(c_file, sizeof(c_file), "%s_gen.c", out_name);
    FILE *f = fopen(c_file, "w");
    if (!f) { fprintf(stderr, "Failed to create C source\n"); exit(1); }

    emit_c_header(f);
    emit_c_functions(prog, f);

    fprintf(f, "\nint main(int argc, char **argv) {\n");
    fprintf(f, "  io_args = val_list();\n");
    fprintf(f, "  for(int i=1; i<argc; i++) { \n");
    fprintf(f, "    if(io_args->item_count >= io_args->item_cap) { io_args->item_cap*=2; io_args->items=realloc(io_args->items, io_args->item_cap*sizeof(Value*)); }\n");
    fprintf(f, "    io_args->items[io_args->item_count++] = val_str(argv[i]);\n");
    fprintf(f, "  }\n");
    fprintf(f, "  Table *env = table_new(NULL);\n");
    fprintf(f, "  table_set(env, \"arg\", io_args);\n");
    emit_c_stmt(prog, 1, f);
    fprintf(f, "  return 0;\n}\n");
    fclose(f);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "gcc -O2 %s -o %s", c_file, out_name);

    printf("Compiling %s...\n", out_name);
    if (system(cmd) != 0) {
        fprintf(stderr, COL_RED "Build failed." COL_RST " Ensure gcc is in your PATH.\n");
        exit(1);
    }
    printf(COL_GRN "Successfully built %s" COL_RST "\n", out_name);
    if (!keep_c) remove(c_file);
}
