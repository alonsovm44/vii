#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* ──────────────────────── Value ──────────────────────── */

typedef enum { VAL_NUM, VAL_STR, VAL_LIST, VAL_NONE } ValKind;

typedef struct Value {
    ValKind kind;
    double num;
    char  *str;
    struct Value **items;   /* list items */
    int    item_count;
    int    item_cap;
} Value;

static Value *val_num(double n) {
    Value *v = calloc(1, sizeof(Value));
    v->kind = VAL_NUM; v->num = n;
    return v;
}

static Value *val_str(const char *s) {
    Value *v = calloc(1, sizeof(Value));
    v->kind = VAL_STR; v->str = strdup(s);
    return v;
}

static Value *val_list(void) {
    Value *v = calloc(1, sizeof(Value));
    v->kind = VAL_LIST; v->item_cap = 8;
    v->items = calloc(v->item_cap, sizeof(Value*));
    return v;
}

static Value *val_none(void) {
    Value *v = calloc(1, sizeof(Value));
    v->kind = VAL_NONE;
    return v;
}

static bool val_truthy(Value *v) {
    if (!v) return false;
    switch (v->kind) {
        case VAL_NUM:  return v->num != 0;
        case VAL_STR:  return v->str[0] != '\0';
        case VAL_LIST: return v->item_count > 0;
        default:       return false;
    }
}

static void val_print(Value *v) {
    if (!v) return;
    switch (v->kind) {
        case VAL_NUM:
            if (v->num == (long long)v->num)
                printf("%lld", (long long)v->num);
            else
                printf("%g", v->num);
            break;
        case VAL_STR:  printf("%s", v->str); break;
        case VAL_LIST:
            putchar('[');
            for (int i = 0; i < v->item_count; i++) {
                if (i) printf(", ");
                val_print(v->items[i]);
            }
            putchar(']');
            break;
        case VAL_NONE: break;
    }
}

/* ──────────────────────── Lexer ──────────────────────── */

typedef enum {
    TOK_NUM, TOK_STR, TOK_IDENT,
    TOK_IF, TOK_ELSE, TOK_WHILE, TOK_DO, TOK_ASK, TOK_LIST, TOK_AT, TOK_SET,
    TOK_EQ, TOK_EQEQ, TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PCT,
    TOK_LT, TOK_GT,
    TOK_NEWLINE, TOK_INDENT, TOK_DEDENT, TOK_EOF
} TokKind;

static const char *tok_kw[] = {
    "if","else","while","do","ask","list","at","set"
};
static const TokKind kw_kind[] = {
    TOK_IF,TOK_ELSE,TOK_WHILE,TOK_DO,TOK_ASK,TOK_LIST,TOK_AT,TOK_SET
};

typedef struct Token {
    TokKind kind;
    char   *text;
    double  num;
    int     line;
} Token;

typedef struct Lexer {
    const char *src;
    int         pos;
    int         line;
    int         indent;         /* current indent level */
    int         indents[256];   /* indent stack */
    int         indent_top;
    bool        at_line_start;
    Token      *tokens;
    int         tok_count;
    int         tok_cap;
} Lexer;

static void lex_push(Lexer *l, Token t) {
    if (l->tok_count >= l->tok_cap) {
        l->tok_cap = l->tok_cap ? l->tok_cap * 2 : 256;
        l->tokens = realloc(l->tokens, l->tok_cap * sizeof(Token));
    }
    l->tokens[l->tok_count++] = t;
}

static void lex_indent(Lexer *l) {
    int spaces = 0;
    while (l->src[l->pos] == ' ') { spaces++; l->pos++; }
    if (l->src[l->pos] == '\n' || l->src[l->pos] == '\0') return; /* blank line */

    if (spaces > l->indent) {
        l->indents[++l->indent_top] = spaces;
        l->indent = spaces;
        lex_push(l, (Token){TOK_INDENT, NULL, 0, l->line});
    }
    while (spaces < l->indent) {
        l->indent_top--;
        l->indent = l->indents[l->indent_top];
        lex_push(l, (Token){TOK_DEDENT, NULL, 0, l->line});
    }
}

static void lex(Lexer *l) {
    l->tok_count = 0;
    l->indent = 0;
    l->indent_top = 0;
    l->indents[0] = 0;
    l->at_line_start = true;
    l->line = 1;

    while (l->src[l->pos]) {
        char c = l->src[l->pos];

        /* newline */
        if (c == '\n') {
            /* collapse multiple newlines */
            if (l->tok_count == 0 || l->tokens[l->tok_count-1].kind != TOK_NEWLINE)
                lex_push(l, (Token){TOK_NEWLINE, NULL, 0, l->line});
            l->line++;
            l->pos++;
            l->at_line_start = true;
            continue;
        }

        /* indent/dedent at line start */
        if (l->at_line_start) {
            if (c == ' ') {
                lex_indent(l);
                l->at_line_start = false;
                continue;
            }
            /* no leading spaces but indent > 0 → dedent */
            while (0 < l->indent) {
                l->indent_top--;
                l->indent = l->indents[l->indent_top];
                lex_push(l, (Token){TOK_DEDENT, NULL, 0, l->line});
            }
            l->at_line_start = false;
        }

        /* skip spaces */
        if (c == ' ' || c == '\t') { l->pos++; continue; }

        /* comment */
        if (c == '#') {
            while (l->src[l->pos] && l->src[l->pos] != '\n') l->pos++;
            continue;
        }

        /* number */
        if (isdigit(c)) {
            int start = l->pos;
            while (isdigit(l->src[l->pos])) l->pos++;
            if (l->src[l->pos] == '.') { l->pos++; while (isdigit(l->src[l->pos])) l->pos++; }
            char *text = malloc(l->pos - start + 1);
            memcpy(text, l->src + start, l->pos - start);
            text[l->pos - start] = '\0';
            double num = atof(text);
            lex_push(l, (Token){TOK_NUM, text, num, l->line});
            continue;
        }

        /* string */
        if (c == '"') {
            l->pos++;
            int start = l->pos;
            while (l->src[l->pos] && l->src[l->pos] != '"') {
                if (l->src[l->pos] == '\\') l->pos++;
                l->pos++;
            }
            char *text = malloc(l->pos - start + 1);
            memcpy(text, l->src + start, l->pos - start);
            text[l->pos - start] = '\0';
            /* unescape */
            char *dst = text;
            for (char *src = text; *src; src++) {
                if (*src == '\\' && *(src+1)) {
                    src++;
                    switch (*src) {
                        case 'n': *dst++ = '\n'; break;
                        case 't': *dst++ = '\t'; break;
                        case '\\': *dst++ = '\\'; break;
                        case '"': *dst++ = '"'; break;
                        default: *dst++ = *src; break;
                    }
                } else {
                    *dst++ = *src;
                }
            }
            *dst = '\0';
            if (l->src[l->pos] == '"') l->pos++;
            lex_push(l, (Token){TOK_STR, text, 0, l->line});
            continue;
        }

        /* operators */
        if (c == '=' && l->src[l->pos+1] == '=') {
            lex_push(l, (Token){TOK_EQEQ, strdup("=="), 0, l->line});
            l->pos += 2; continue;
        }
        if (c == '=') { lex_push(l, (Token){TOK_EQ, strdup("="), 0, l->line}); l->pos++; continue; }
        if (c == '+') { lex_push(l, (Token){TOK_PLUS, strdup("+"), 0, l->line}); l->pos++; continue; }
        if (c == '-') { lex_push(l, (Token){TOK_MINUS, strdup("-"), 0, l->line}); l->pos++; continue; }
        if (c == '*') { lex_push(l, (Token){TOK_STAR, strdup("*"), 0, l->line}); l->pos++; continue; }
        if (c == '/') { lex_push(l, (Token){TOK_SLASH, strdup("/"), 0, l->line}); l->pos++; continue; }
        if (c == '%') { lex_push(l, (Token){TOK_PCT, strdup("%"), 0, l->line}); l->pos++; continue; }
        if (c == '<') { lex_push(l, (Token){TOK_LT, strdup("<"), 0, l->line}); l->pos++; continue; }
        if (c == '>') { lex_push(l, (Token){TOK_GT, strdup(">"), 0, l->line}); l->pos++; continue; }

        /* identifier / keyword */
        if (isalpha(c) || c == '_') {
            int start = l->pos;
            while (isalnum(l->src[l->pos]) || l->src[l->pos] == '_') l->pos++;
            int len = l->pos - start;
            char *text = malloc(len + 1);
            memcpy(text, l->src + start, len);
            text[len] = '\0';
            TokKind kind = TOK_IDENT;
            for (int i = 0; i < 8; i++) {
                if (strcmp(text, tok_kw[i]) == 0) { kind = kw_kind[i]; break; }
            }
            lex_push(l, (Token){kind, text, 0, l->line});
            continue;
        }

        fprintf(stderr, "Error: unexpected character '%c' on line %d\n", c, l->line);
        l->pos++;
    }

    /* emit remaining dedents */
    while (l->indent > 0) {
        l->indent_top--;
        l->indent = l->indents[l->indent_top];
        lex_push(l, (Token){TOK_DEDENT, NULL, 0, l->line});
    }
    lex_push(l, (Token){TOK_EOF, NULL, 0, l->line});
}

/* ──────────────────────── AST ──────────────────────── */

typedef enum {
    ND_NUM, ND_STR, ND_VAR,
    ND_ASSIGN, ND_BINOP,
    ND_IF, ND_WHILE, ND_DO,
    ND_ASK, ND_LIST, ND_AT,
    ND_CALL, ND_BLOCK,
    ND_PRINT  /* implicit print */
} NdKind;

typedef struct Node {
    NdKind kind;
    char  *name;        /* variable / function name */
    double num;         /* number literal */
    char  *str;         /* string literal */
    TokKind op;         /* binary operator */
    struct Node *left;  /* assignment target, condition, list expr, call func */
    struct Node *right; /* assignment value, else-branch, at index, set value */
    struct Node **body; /* block body / call args / at list / set list */
    int    body_count;
    int    body_cap;
} Node;

static Node *nd_new(NdKind kind) {
    Node *n = calloc(1, sizeof(Node));
    n->kind = kind;
    return n;
}

static void nd_push(Node *block, Node *child) {
    if (block->body_count >= block->body_cap) {
        block->body_cap = block->body_cap ? block->body_cap * 2 : 8;
        block->body = realloc(block->body, block->body_cap * sizeof(Node*));
    }
    block->body[block->body_count++] = child;
}

/* ──────────────────────── Parser ──────────────────────── */

typedef struct Parser {
    Token *tokens;
    int    pos;
} Parser;

static Token *peek(Parser *p) { return &p->tokens[p->pos]; }
static Token *advance(Parser *p) { return &p->tokens[p->pos++]; }

static void expect(Parser *p, TokKind k) {
    if (peek(p)->kind != k) {
        fprintf(stderr, "Parse error: expected token kind %d, got %d on line %d\n",
                k, peek(p)->kind, peek(p)->line);
        exit(1);
    }
    advance(p);
}

static void skip_newlines(Parser *p) {
    while (peek(p)->kind == TOK_NEWLINE) advance(p);
}

/* forward declarations */
static Node *parse_expr(Parser *p);
static Node *parse_block(Parser *p);

static Node *parse_primary(Parser *p) {
    Token *t = peek(p);
    switch (t->kind) {
        case TOK_NUM:   advance(p); { Node *n = nd_new(ND_NUM); n->num = t->num; return n; }
        case TOK_STR:   advance(p); { Node *n = nd_new(ND_STR); n->str = strdup(t->text); return n; }
        case TOK_IDENT: advance(p); { Node *n = nd_new(ND_VAR); n->name = strdup(t->text); return n; }
        case TOK_ASK:   advance(p); return nd_new(ND_ASK);
        case TOK_LIST:  advance(p); return nd_new(ND_LIST);
        case TOK_DO: {
            advance(p);
            Node *fn = nd_new(ND_DO);
            fn->name = strdup(advance(p)->text); /* function name */
            /* parameters */
            fn->body_cap = 8;
            fn->body = calloc(fn->body_cap, sizeof(Node*));
            while (peek(p)->kind == TOK_IDENT) {
                Node *param = nd_new(ND_VAR);
                param->name = strdup(advance(p)->text);
                nd_push(fn, param);
            }
            skip_newlines(p);
            expect(p, TOK_INDENT);
            fn->left = parse_block(p);
            expect(p, TOK_DEDENT);
            return fn;
        }
        default:
            fprintf(stderr, "Parse error: unexpected token '%s' (kind %d) on line %d\n",
                    t->text ? t->text : "", t->kind, t->line);
            exit(1);
    }
}

static Node *parse_postfix(Parser *p) {
    Node *expr = parse_primary(p);

    while (true) {
        if (peek(p)->kind == TOK_AT) {
            advance(p);
            Node *at = nd_new(ND_AT);
            at->left = expr;
            at->right = parse_postfix(p);  /* index - only primary/postfix, not full expr */
            expr = at;
        } else if (peek(p)->kind == TOK_IDENT || peek(p)->kind == TOK_NUM ||
                   peek(p)->kind == TOK_STR || peek(p)->kind == TOK_ASK ||
                   peek(p)->kind == TOK_LIST) {
            /* function call: funcname arg1 arg2 ... */
            /* only if expr is a variable (function name) */
            if (expr->kind != ND_VAR) break;
            /* peek ahead: if next token could start an expression, it's a call */
            /* but we need to stop at operators, newlines, etc. */
            /* A call consumes arguments until we hit something that isn't a primary */
            Node *call = nd_new(ND_CALL);
            call->left = expr;  /* function ref */
            call->body_cap = 8;
            call->body = calloc(call->body_cap, sizeof(Node*));
            /* consume arguments */
            while (peek(p)->kind == TOK_IDENT || peek(p)->kind == TOK_NUM ||
                   peek(p)->kind == TOK_STR || peek(p)->kind == TOK_ASK ||
                   peek(p)->kind == TOK_LIST) {
                nd_push(call, parse_primary(p));
            }
            expr = call;
        } else {
            break;
        }
    }
    return expr;
}

static Node *parse_expr(Parser *p) {
    Node *left = parse_postfix(p);

    while (peek(p)->kind == TOK_PLUS  || peek(p)->kind == TOK_MINUS ||
           peek(p)->kind == TOK_STAR  || peek(p)->kind == TOK_SLASH ||
           peek(p)->kind == TOK_PCT   || peek(p)->kind == TOK_LT    ||
           peek(p)->kind == TOK_GT    || peek(p)->kind == TOK_EQEQ) {
        TokKind op = advance(p)->kind;
        Node *right = parse_postfix(p);
        Node *bin = nd_new(ND_BINOP);
        bin->op = op;
        bin->left = left;
        bin->right = right;
        left = bin;
    }

    /* assignment: var = expr */
    if (peek(p)->kind == TOK_EQ && (left->kind == ND_VAR || left->kind == ND_AT)) {
        advance(p);
        Node *assign = nd_new(ND_ASSIGN);
        assign->left = left;
        assign->right = parse_expr(p);
        return assign;
    }

    return left;
}

static Node *parse_stmt(Parser *p) {
    skip_newlines(p);
    Token *t = peek(p);

    if (t->kind == TOK_IF) {
        advance(p);
        Node *n = nd_new(ND_IF);
        n->left = parse_expr(p);  /* condition */
        skip_newlines(p);
        expect(p, TOK_INDENT);
        n->body = NULL; n->body_count = 0; n->body_cap = 0;
        nd_push(n, parse_block(p));
        expect(p, TOK_DEDENT);
        /* else */
        skip_newlines(p);
        if (peek(p)->kind == TOK_ELSE) {
            advance(p);
            skip_newlines(p);
            if (peek(p)->kind == TOK_IF) {
                /* else if → single-statement else branch */
                n->right = parse_stmt(p);
            } else {
                expect(p, TOK_INDENT);
                n->right = parse_block(p);
                expect(p, TOK_DEDENT);
            }
        }
        return n;
    }

    if (t->kind == TOK_WHILE) {
        advance(p);
        Node *n = nd_new(ND_WHILE);
        n->left = parse_expr(p);
        skip_newlines(p);
        expect(p, TOK_INDENT);
        n->body = NULL; n->body_count = 0; n->body_cap = 0;
        nd_push(n, parse_block(p));
        expect(p, TOK_DEDENT);
        return n;
    }

    /* expression statement (with implicit print) */
    Node *expr = parse_expr(p);
    /* if it's not an assignment, wrap in implicit print */
    if (expr->kind != ND_ASSIGN && expr->kind != ND_DO) {
        Node *print = nd_new(ND_PRINT);
        print->left = expr;
        return print;
    }
    return expr;
}

static Node *parse_block(Parser *p) {
    Node *block = nd_new(ND_BLOCK);
    while (peek(p)->kind != TOK_DEDENT && peek(p)->kind != TOK_EOF) {
        if (peek(p)->kind == TOK_NEWLINE) { advance(p); continue; }
        nd_push(block, parse_stmt(p));
        skip_newlines(p);
    }
    return block;
}

static Node *parse_program(Parser *p) {
    Node *prog = nd_new(ND_BLOCK);
    while (peek(p)->kind != TOK_EOF) {
        if (peek(p)->kind == TOK_NEWLINE) { advance(p); continue; }
        nd_push(prog, parse_stmt(p));
        skip_newlines(p);
    }
    return prog;
}

/* ──────────────────────── Interpreter ──────────────────────── */

typedef struct Entry {
    char       *key;
    Value      *val;
    struct Entry *next;
} Entry;

#define TABLE_SIZE 256

typedef struct Table {
    Entry      *buckets[TABLE_SIZE];
    struct Table *parent;
} Table;

static unsigned hash(const char *s) {
    unsigned h = 0;
    while (*s) h = h * 31 + (unsigned char)*s++;
    return h % TABLE_SIZE;
}

static Table *table_new(Table *parent) {
    Table *t = calloc(1, sizeof(Table));
    t->parent = parent;
    return t;
}

static Value *table_get(Table *t, const char *key) {
    for (Table *cur = t; cur; cur = cur->parent) {
        unsigned h = hash(key);
        for (Entry *e = cur->buckets[h]; e; e = e->next) {
            if (strcmp(e->key, key) == 0) return e->val;
        }
    }
    return NULL;
}

static void table_set(Table *t, const char *key, Value *val) {
    unsigned h = hash(key);
    for (Entry *e = t->buckets[h]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) { e->val = val; return; }
    }
    Entry *e = malloc(sizeof(Entry));
    e->key = strdup(key);
    e->val = val;
    e->next = t->buckets[h];
    t->buckets[h] = e;
}

/* function storage */
typedef struct Func {
    char  *name;
    Node  *def;  /* ND_DO node */
    struct Func *next;
} Func;

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

/* forward declaration */
static Value *eval(Node *n, Table *env);

static Value *eval_block(Node *block, Table *env) {
    Value *last = val_none();
    for (int i = 0; i < block->body_count; i++) {
        last = eval(block->body[i], env);
    }
    return last;
}

static Value *eval(Node *n, Table *env) {
    if (!n) return val_none();

    switch (n->kind) {
        case ND_NUM:   return val_num(n->num);
        case ND_STR:   return val_str(n->str);
        case ND_VAR:   {
            Value *v = table_get(env, n->name);
            if (!v) { fprintf(stderr, "Runtime error: undefined variable '%s'\n", n->name); exit(1); }
            return v;
        }
        case ND_ASSIGN: {
            Value *val = eval(n->right, env);
            if (n->left->kind == ND_VAR) {
                table_set(env, n->left->name, val);
            } else if (n->left->kind == ND_AT) {
                /* list at index = value */
                Value *list = eval(n->left->left, env);
                Value *idx  = eval(n->left->right, env);
                if (list->kind != VAL_LIST) { fprintf(stderr, "Runtime error: 'at' on non-list\n"); exit(1); }
                int i = (int)idx->num;
                if (i < 0) i += list->item_count;
                /* allow append at index == item_count */
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
            Value *l = eval(n->left, env);
            Value *r = eval(n->right, env);
            double a = l->num, b = r->num;
            /* handle string concatenation */
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
                case TOK_PCT:   return val_num((int)a % (int)b);
                case TOK_LT:    return val_num(a < b ? 1 : 0);
                case TOK_GT:    return val_num(a > b ? 1 : 0);
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
            if (val_truthy(cond)) {
                if (n->body_count > 0) {
                    for (int i = 0; i < n->body_count; i++)
                        eval(n->body[i], env);
                }
            } else if (n->right) {
                if (n->right->kind == ND_BLOCK) {
                    eval_block(n->right, env);
                } else {
                    eval(n->right, env);
                }
            }
            return val_none();
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
            /* try to parse as number */
            char *end;
            double d = strtod(buf, &end);
            if (*end == '\0' && end != buf) return val_num(d);
            return val_str(buf);
        }
        case ND_LIST: {
            return val_list();
        }
        case ND_AT: {
            Value *list = eval(n->left, env);
            Value *idx  = eval(n->right, env);
            if (list->kind != VAL_LIST) { fprintf(stderr, "Runtime error: 'at' on non-list\n"); exit(1); }
            int i = (int)idx->num;
            if (i < 0) i += list->item_count;
            if (i < 0 || i >= list->item_count) { fprintf(stderr, "Runtime error: index %d out of range\n", i); exit(1); }
            return list->items[i];
        }
        case ND_CALL: {
            const char *fname = n->left->name;
            Func *fn = func_find(fname);
            if (!fn) { fprintf(stderr, "Runtime error: undefined function '%s'\n", fname); exit(1); }
            /* evaluate arguments */
            int argc = n->body_count;
            Value **argv = malloc(argc * sizeof(Value*));
            for (int i = 0; i < argc; i++)
                argv[i] = eval(n->body[i], env);
            /* create new scope */
            Table *scope = table_new(env);
            /* bind params */
            for (int i = 0; i < fn->def->body_count && i < argc; i++) {
                table_set(scope, fn->def->body[i]->name, argv[i]);
            }
            /* execute body, implicit return last value */
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

/* ──────────────────────── Main ──────────────────────── */

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open '%s'\n", path); exit(1); }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: io <file.io>\n");
        return 1;
    }

    char *src = read_file(argv[1]);

    Lexer lexer = { .src = src, .pos = 0 };
    lex(&lexer);

    Parser parser = { .tokens = lexer.tokens, .pos = 0 };
    Node *prog = parse_program(&parser);

    Table *global = table_new(NULL);
    eval(prog, global);

    free(src);
    return 0;
}
