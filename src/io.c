#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* ──────────────────────── UI & Colors ──────────────────────── */

#define COL_RED   "\x1b[1;31m"
#define COL_GRN   "\x1b[1;32m"
#define COL_YEL   "\x1b[1;33m"
#define COL_RST   "\x1b[0m"

static void enable_ansi_colors(void) {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) {
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
}

static void report_error(const char *filename, const char *src, int pos, int line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, COL_RED "Error in %s on line %d: " COL_RST, filename, line);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");

    int start = pos;
    while (start > 0 && src[start - 1] != '\n' && src[start - 1] != '\r') start--;
    int end = pos;
    while (src[end] && src[end] != '\n' && src[end] != '\r') end++;

    fprintf(stderr, " %5d | %.*s\n", line, end - start, src + start);
    fprintf(stderr, "       | ");
    int caret = pos - start;
    /* account for \r chars before pos on this line */
    for (int i = start; i < pos; i++) { if (src[i] == '\r') caret--; }
    for (int i = 0; i < caret; i++) fprintf(stderr, " ");
    fprintf(stderr, COL_GRN "^" COL_RST "\n");
    exit(1);
}

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

static void val_print_to(Value *v, FILE *f) {
    if (!v) return;
    switch (v->kind) {
        case VAL_NUM:
            if (v->num == (long long)v->num)
                fprintf(f, "%lld", (long long)v->num);
            else
                fprintf(f, "%g", v->num);
            break;
        case VAL_STR:  fprintf(f, "%s", v->str); break;
        case VAL_LIST:
            fputc('[', f);
            for (int i = 0; i < v->item_count; i++) {
                if (i) fprintf(f, ", ");
                val_print_to(v->items[i], f);
            }
            fputc(']', f);
            break;
        case VAL_NONE: break;
    }
}

static void val_print(Value *v) {
    val_print_to(v, stdout);
}

/* ──────────────────────── Lexer ──────────────────────── */

typedef enum {
    TOK_NUM, TOK_STR, TOK_IDENT,
    TOK_IF, TOK_ELSE, TOK_WHILE, TOK_DO, TOK_ASK, TOK_LIST, TOK_AT, TOK_SET,
    TOK_PUT, TOK_ARG, TOK_PASTE, TOK_LEN, TOK_ORD, TOK_CHR, TOK_TONUM, TOK_TOSTR,
    TOK_EQ, TOK_EQEQ, TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PCT,
    TOK_LT, TOK_GT,
    TOK_NEWLINE, TOK_INDENT, TOK_DEDENT, TOK_EOF
} TokKind;

static const char *tok_kw[] = {
    "if","else","while","do","ask","list","at","set","put","arg","paste","len","ord","chr","tonum","tostr"
};
static const TokKind kw_kind[] = {
    TOK_IF,TOK_ELSE,TOK_WHILE,TOK_DO,TOK_ASK,TOK_LIST,TOK_AT,TOK_SET,TOK_PUT,TOK_ARG,TOK_PASTE,TOK_LEN,TOK_ORD,TOK_CHR,TOK_TONUM,TOK_TOSTR
};
#define KW_COUNT 16

typedef struct Token {
    TokKind kind;
    char   *text;
    double  num;
    int     line;
    int     pos;
} Token;

typedef struct Lexer {
    const char *src;
    int         pos;
    int         line;
    const char *filename;
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

static void lex_indent(Lexer *l) { // No filename needed here, as it's internal to lexing
    int spaces = 0;
    while (l->src[l->pos] == ' ') { spaces++; l->pos++; }
    if (l->src[l->pos] == '\r') l->pos++;  /* skip \r in CRLF */
    if (l->src[l->pos] == '\n' || l->src[l->pos] == '\0') return; /* blank line */

    if (spaces > l->indent) {
        l->indents[++l->indent_top] = spaces;
        l->indent = spaces;
        lex_push(l, (Token){TOK_INDENT, NULL, 0, l->line, l->pos});
    }
    while (spaces < l->indent) {
        l->indent_top--;
        l->indent = l->indents[l->indent_top];
        lex_push(l, (Token){TOK_DEDENT, NULL, 0, l->line, l->pos});
    }
}

static void lex(Lexer *l, const char *filename) {
    l->tok_count = 0;
    l->indent = 0;
    l->indent_top = 0;
    l->indents[0] = 0;
    l->at_line_start = true;
    l->line = 1;
    l->filename = filename;

    while (l->src[l->pos]) {
        char c = l->src[l->pos];

        /* skip \r (CRLF) */
        if (c == '\r') { l->pos++; continue; }

        /* newline */
        if (c == '\n') {
            /* collapse multiple newlines */
            if (l->tok_count == 0 || l->tokens[l->tok_count-1].kind != TOK_NEWLINE)
                lex_push(l, (Token){TOK_NEWLINE, NULL, 0, l->line, l->pos});
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
                lex_push(l, (Token){TOK_DEDENT, NULL, 0, l->line, l->pos});
            }
            l->at_line_start = false;
        }

        /* skip spaces */
        if (c == ' ' || c == '\t') { l->pos++; continue; }

        /* comment */
        if (c == '#') {
            while (l->src[l->pos] && l->src[l->pos] != '\n' && l->src[l->pos] != '\r') l->pos++;
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
            lex_push(l, (Token){TOK_NUM, text, num, l->line, start});
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
            lex_push(l, (Token){TOK_STR, text, 0, l->line, start});
            continue;
        }

        /* operators */
        if (c == '=' && l->src[l->pos+1] == '=') {
            lex_push(l, (Token){TOK_EQEQ, strdup("=="), 0, l->line, l->pos});
            l->pos += 2; continue;
        }
        if (c == '=') { lex_push(l, (Token){TOK_EQ, strdup("="), 0, l->line, l->pos}); l->pos++; continue; }
        if (c == '+') { lex_push(l, (Token){TOK_PLUS, strdup("+"), 0, l->line, l->pos}); l->pos++; continue; }
        if (c == '-') { lex_push(l, (Token){TOK_MINUS, strdup("-"), 0, l->line, l->pos}); l->pos++; continue; }
        if (c == '*') { lex_push(l, (Token){TOK_STAR, strdup("*"), 0, l->line, l->pos}); l->pos++; continue; }
        if (c == '/') { lex_push(l, (Token){TOK_SLASH, strdup("/"), 0, l->line, l->pos}); l->pos++; continue; }
        if (c == '%') { lex_push(l, (Token){TOK_PCT, strdup("%"), 0, l->line, l->pos}); l->pos++; continue; }
        if (c == '<') { lex_push(l, (Token){TOK_LT, strdup("<"), 0, l->line, l->pos}); l->pos++; continue; }
        if (c == '>') { lex_push(l, (Token){TOK_GT, strdup(">"), 0, l->line, l->pos}); l->pos++; continue; }

        /* identifier / keyword */
        if (isalpha(c) || c == '_') {
            int start = l->pos;
            while (isalnum(l->src[l->pos]) || l->src[l->pos] == '_') l->pos++;
            int len = l->pos - start;
            char *text = malloc(len + 1);
            memcpy(text, l->src + start, len);
            text[len] = '\0';
            TokKind kind = TOK_IDENT;
            for (int i = 0; i < KW_COUNT; i++) {
                if (strcmp(text, tok_kw[i]) == 0) { kind = kw_kind[i]; break; }
            }
            lex_push(l, (Token){kind, text, 0, l->line, start});
            continue;
        }

        report_error(l->filename, l->src, l->pos, l->line, "unexpected character '%c'", c);
    }

    /* emit remaining dedents */
    while (l->indent > 0) {
        l->indent_top--;
        l->indent = l->indents[l->indent_top];
        lex_push(l, (Token){TOK_DEDENT, NULL, 0, l->line, l->pos});
    }
    lex_push(l, (Token){TOK_EOF, NULL, 0, l->line, l->pos});
}

/* ──────────────────────── AST ──────────────────────── */

typedef enum {
    ND_NUM, ND_STR, ND_VAR,
    ND_ASSIGN, ND_BINOP,
    ND_IF, ND_WHILE, ND_DO,
    ND_ASK, ND_ASKFILE, ND_LIST, ND_AT, ND_SET,
    ND_PUT, ND_ARG, ND_LEN, ND_ORD, ND_CHR, ND_TONUM, ND_TOSTR,
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

/* forward decl for paste */
static char *read_file(const char *path);

/* ──────────────────────── Parser ──────────────────────── */

typedef struct Parser {
    Token *tokens;
    int    pos;
    const char *filename;
    const char *src;
} Parser;

static Token *peek(Parser *p) { return &p->tokens[p->pos]; }
static Token *advance(Parser *p) { return &p->tokens[p->pos++]; }

static void expect(Parser *p, TokKind k) {
    if (peek(p)->kind != k) {
        report_error(p->filename, p->src, peek(p)->pos, peek(p)->line, "expected token kind %d, got %d", k, peek(p)->kind);
    }
    advance(p);
}

static void skip_newlines(Parser *p) {
    while (peek(p)->kind == TOK_NEWLINE) advance(p);
}

/* forward declarations */
static Node *parse_postfix(Parser *p);
static Node *parse_expr(Parser *p);
static Node *parse_block(Parser *p);
static Node *parse_program(Parser *p);

static Node *parse_primary(Parser *p) {
    Token *t = peek(p);
    switch (t->kind) {
        case TOK_NUM:   advance(p); { Node *n = nd_new(ND_NUM); n->num = t->num; return n; }
        case TOK_STR:   advance(p); { Node *n = nd_new(ND_STR); n->str = strdup(t->text); return n; }
        case TOK_IDENT: advance(p); { Node *n = nd_new(ND_VAR); n->name = strdup(t->text); return n; }
        case TOK_ASK:   advance(p); return nd_new(ND_ASK);
        case TOK_ARG:   advance(p); return nd_new(ND_ARG);
        case TOK_LEN:   advance(p); { Node *n = nd_new(ND_LEN);   n->left = parse_postfix(p); return n; }
        case TOK_ORD:   advance(p); { Node *n = nd_new(ND_ORD);   n->left = parse_postfix(p); return n; }
        case TOK_CHR:   advance(p); { Node *n = nd_new(ND_CHR);   n->left = parse_postfix(p); return n; }
        case TOK_TONUM: advance(p); { Node *n = nd_new(ND_TONUM); n->left = parse_postfix(p); return n; }
        case TOK_TOSTR: advance(p); { Node *n = nd_new(ND_TOSTR); n->left = parse_postfix(p); return n; }
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
            report_error(p->filename, p->src, t->pos, t->line, "unexpected token '%s'", t->text ? t->text : "");
    }
}

static Node *parse_postfix(Parser *p) {
    Node *expr = parse_primary(p);

    while (true) {
        if (peek(p)->kind == TOK_AT) {
            advance(p);
            Node *at = nd_new(ND_AT);
            at->left = expr;
            at->right = parse_postfix(p);  /* index */
            expr = at;
        } else if (peek(p)->kind == TOK_SET) {
            /* list set index value */
            advance(p);
            Node *set = nd_new(ND_SET);
            set->left = expr;  /* the list */
            set->body_cap = 4;
            set->body = calloc(set->body_cap, sizeof(Node*));
            nd_push(set, parse_postfix(p));  /* index */
            nd_push(set, parse_expr(p));     /* value */
            expr = set;
        } else if (peek(p)->kind == TOK_ASK && expr->kind != ND_ASK) {
            /* "file.txt" ask — file read */
            advance(p);
            Node *af = nd_new(ND_ASKFILE);
            af->left = expr;  /* path expression */
            expr = af;
        } else if (peek(p)->kind == TOK_IDENT || peek(p)->kind == TOK_NUM ||
                   peek(p)->kind == TOK_STR || peek(p)->kind == TOK_ASK ||
                   peek(p)->kind == TOK_LIST || peek(p)->kind == TOK_ARG ||
                   peek(p)->kind == TOK_LEN || peek(p)->kind == TOK_ORD ||
                   peek(p)->kind == TOK_CHR || peek(p)->kind == TOK_TONUM ||
                   peek(p)->kind == TOK_TOSTR) {
            /* function call: funcname arg1 arg2 ... */
            if (expr->kind != ND_VAR) break;
            Node *call = nd_new(ND_CALL);
            call->left = expr;
            call->body_cap = 8;
            call->body = calloc(call->body_cap, sizeof(Node*));
            while (peek(p)->kind == TOK_IDENT || peek(p)->kind == TOK_NUM ||
                   peek(p)->kind == TOK_STR || peek(p)->kind == TOK_ASK ||
                   peek(p)->kind == TOK_LIST || peek(p)->kind == TOK_ARG ||
                   peek(p)->kind == TOK_LEN || peek(p)->kind == TOK_ORD ||
                   peek(p)->kind == TOK_CHR || peek(p)->kind == TOK_TONUM ||
                   peek(p)->kind == TOK_TOSTR) {
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

    if (t->kind == TOK_PUT) {
        advance(p);
        Node *put = nd_new(ND_PUT);
        put->left = parse_expr(p);   /* path */
        put->right = parse_expr(p);  /* data */
        return put;
    }

    if (t->kind == TOK_PASTE) {
        advance(p);
        if (peek(p)->kind != TOK_STR) {
            report_error(p->filename, p->src, peek(p)->pos, peek(p)->line, "paste requires a string filename");
        }
        char *filename = strdup(peek(p)->text);
        advance(p);
        char *src = read_file(filename);
        Lexer sub_lex = { .src = src, .pos = 0, .filename = filename };
        lex(&sub_lex, filename);
        Parser sub_p = { .tokens = sub_lex.tokens, .pos = 0, .src = src, .filename = filename };
        Node *included = parse_program(&sub_p);
        free(filename);
        free(src);
        return included;
    }

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
    if (expr->kind != ND_ASSIGN && expr->kind != ND_DO && expr->kind != ND_SET && expr->kind != ND_PUT) {
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

/* CLI arguments (set in main) */
static Value *cli_args = NULL;

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
                Value *list = eval(n->left->left, env); // This is the list itself
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
                case TOK_SLASH: return val_num(b != 0 ? (double)((long long)a / (long long)b) : 0);
                case TOK_PCT:   return val_num((long long)a % (long long)b);
                case TOK_LT:    return val_num(a < b ? -1 : 0);
                case TOK_GT:    return val_num(a > b ? -1 : 0);
                case TOK_EQEQ:  {
                    if (l->kind == VAL_STR && r->kind == VAL_STR)
                        return val_num(strcmp(l->str, r->str) == 0 ? -1 : 0);
                    return val_num(a == b ? -1 : 0);
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
            /* keyboard input */
            char buf[1024];
            if (!fgets(buf, sizeof(buf), stdin)) return val_str("");
            buf[strcspn(buf, "\n")] = '\0';
            char *end;
            double d = strtod(buf, &end);
            if (*end == '\0' && end != buf) return val_num(d);
            return val_str(buf);
        }
        case ND_ASKFILE: {
            /* file read: "path.txt" ask */
            Value *path = eval(n->left, env);
            if (path->kind != VAL_STR) { fprintf(stderr, "Runtime error: ask requires a string path\n"); exit(1); }
            FILE *f = fopen(path->str, "rb");
            if (!f) { fprintf(stderr, "Runtime error: cannot open '%s'\n", path->str); exit(1); }
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
                /* string at index → single character */
                int i = (int)idx->num;
                if (i < 0) i += (int)strlen(list->str);
                if (i < 0 || i >= (int)strlen(list->str)) { fprintf(stderr, "Runtime error: index %d out of range\n", i); exit(1); }
                char buf[2] = { list->str[i], '\0' };
                return val_str(buf);
            }
            if (list->kind != VAL_LIST) { fprintf(stderr, "Runtime error: 'at' on non-list\n"); exit(1); }
            int i = (int)idx->num;
            if (i < 0) i += list->item_count;
            if (i < 0 || i >= list->item_count) { fprintf(stderr, "Runtime error: index %d out of range\n", i); exit(1); }
            return list->items[i];
        }
        case ND_SET: {
            /* list set index value */
            Value *list = eval(n->left, env);
            if (list->kind != VAL_LIST) { fprintf(stderr, "Runtime error: 'set' on non-list\n"); exit(1); }
            Value *idx  = eval(n->body[0], env);
            Value *val  = eval(n->body[1], env);
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
            return val;
        }
        case ND_PUT: {
            /* put "path" "data" */
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

/* ──────────────────────── Codegen (IO -> C) ──────────────────────── */

static void emit_c_header(FILE *f) {
    fprintf(f, "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <stdbool.h>\n\n");
    fprintf(f, "typedef enum { VAL_NUM, VAL_STR, VAL_LIST, VAL_FUNC, VAL_NONE } ValKind;\n");
    fprintf(f, "struct Table;\nstruct Value;\n");
    fprintf(f, "typedef struct Value* (*IoFunc)(struct Table*, int, struct Value**);\n");
    fprintf(f, "typedef struct Value { ValKind kind; double num; char *str; struct Value **items; int item_count; int item_cap; IoFunc func; } Value;\n");
    fprintf(f, "typedef struct Entry { char *key; Value *val; struct Entry *next; } Entry;\n");
    fprintf(f, "typedef struct Table { Entry *buckets[256]; struct Table *parent; } Table;\n\n");
    
    /* Allocation Helpers */
    fprintf(f, "static Value *val_num(double n) { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_NUM; v->num = n; return v; }\n");
    fprintf(f, "static Value *val_str(const char *s) { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_STR; v->str = strdup(s); return v; }\n");
    fprintf(f, "static Value *val_list() { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_LIST; v->item_cap = 8; v->items = calloc(8, sizeof(Value*)); return v; }\n");
    fprintf(f, "static Value *val_func(IoFunc f) { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_FUNC; v->func = f; return v; }\n");
    fprintf(f, "static Value *val_none() { Value *v = calloc(1, sizeof(Value)); v->kind = VAL_NONE; return v; }\n\n");
    
    /* Runtime Table Logic */
    fprintf(f, "static unsigned hash(const char *s) { unsigned h = 0; while(*s) h = h * 31 + (unsigned char)*s++; return h %% 256; }\n");
    fprintf(f, "static Table* table_new(Table *p) { Table *t = calloc(1, sizeof(Table)); t->parent = p; return t; }\n");
    fprintf(f, "static Value* table_get(Table *t, const char *k) { for(Table *cur=t;cur;cur=cur->parent){ unsigned h=hash(k); for(Entry *e=cur->buckets[h];e;e=e->next) if(!strcmp(e->key,k)) return e->val; } return val_none(); }\n");
    fprintf(f, "static void table_set(Table *t, const char *k, Value *v) { unsigned h = hash(k); for(Entry *e=t->buckets[h];e;e=e->next) if(!strcmp(e->key,k)){e->val=v;return;} Entry *e=malloc(sizeof(Entry)); e->key=strdup(k); e->val=v; e->next=t->buckets[h]; t->buckets[h]=e; }\n\n");

    /* Builtin Operations */
    fprintf(f, "static bool val_truthy(Value *v) { if(!v) return false; if(v->kind==VAL_NUM) return v->num != 0; if(v->kind==VAL_STR) return v->str[0]!='\\0'; return v->item_count > 0; }\n");
    fprintf(f, "static void val_print(Value *v) { if(!v) return; if(v->kind==VAL_NUM) printf(v->num==(long long)v->num?\"%%lld\":\"%%g\",(long long)v->num); else if(v->kind==VAL_STR) printf(\"%%s\",v->str); else if(v->kind==VAL_LIST){ printf(\"[\"); for(int i=0;i<v->item_count;i++){ if(i)printf(\", \"); val_print(v->items[i]); } printf(\"]\"); } }\n");
    fprintf(f, "static Value* runtime_binop(int op, Value* l, Value* r) {\n");
    fprintf(f, "  if(op==16 && (l->kind==VAL_STR||r->kind==VAL_STR)){ char b[1024]; sprintf(b, \"%%s%%s\", l->kind==VAL_STR?l->str:\"\", r->kind==VAL_STR?r->str:\"\"); return val_str(b); }\n");
    fprintf(f, "  if(op==15){ if(l->kind==VAL_STR&&r->kind==VAL_STR) return val_num(strcmp(l->str,r->str)==0?-1:0); }\n");
    fprintf(f, "  double a=l->num, b=r->num; switch(op){\n");
    fprintf(f, "    case 16: return val_num(a+b); case 17: return val_num(a-b); case 18: return val_num(a*b); case 19: return val_num(b?(int)a/(int)b:0); case 20: return val_num(b?(int)a%%(int)b:0);\n");
    fprintf(f, "    case 21: return val_num(a<b?-1:0); case 22: return val_num(a>b?-1:0); case 15: return val_num(a==b?-1:0); default: return val_none();\n");
    fprintf(f, "  }\n}\n");
    fprintf(f, "static Value* runtime_call(Value* f, Table* e, int c, Value** v) { if(f->kind==VAL_FUNC) return f->func(e, c, v); return val_none(); }\n");
    fprintf(f, "static Value* runtime_at(Value* l, Value* r) { int i=(int)r->num; if(l->kind==VAL_STR){ if(i<0||i>=strlen(l->str)) return val_none(); char b[2]={l->str[i],0}; return val_str(b); } if(l->kind==VAL_LIST){ if(i<0||i>=l->item_count) return val_none(); return l->items[i]; } return val_none(); }\n");
    fprintf(f, "static void runtime_set(Value* l, Value* i, Value* v) { if(l->kind!=VAL_LIST) return; int idx=(int)i->num; if(idx==l->item_count){ if(l->item_count>=l->item_cap){ l->item_cap*=2; l->items=realloc(l->items,l->item_cap*sizeof(Value*)); } l->items[l->item_count++]=v; } else if(idx>=0 && idx<l->item_count) l->items[idx]=v; }\n");
    fprintf(f, "static Value* runtime_ask(Value* p) { if(p&&p->kind==VAL_STR){ FILE* f=fopen(p->str,\"rb\"); if(!f) return val_str(\"\"); fseek(f,0,SEEK_END); long l=ftell(f); fseek(f,0,SEEK_SET); char* b=malloc(l+1); fread(b,1,l,f); b[l]=0; fclose(f); Value* v=val_str(b); free(b); return v; } char buf[1024]; if(!fgets(buf,1024,stdin)) return val_str(\"\"); buf[strcspn(buf,\"\\n\")]=0; return val_str(buf); }\n");
    fprintf(f, "static Value* runtime_len(Value* v) { if(v->kind==VAL_STR) return val_num(strlen(v->str)); if(v->kind==VAL_LIST) return val_num(v->item_count); return val_num(0); }\n");
    fprintf(f, "static Value* runtime_ord(Value* v) { if(v->kind==VAL_STR&&v->str[0]) return val_num((unsigned char)v->str[0]); return val_num(0); }\n");
    fprintf(f, "static Value* runtime_chr(Value* v) { char b[2]={v->num,0}; return val_str(b); }\n");
    fprintf(f, "static Value* runtime_tonum(Value* v) { if(v->kind==VAL_STR){ char*e; double d=strtod(v->str,&e); return val_num(*e==0&&e!=v->str?d:0); } if(v->kind==VAL_NUM) return v; return val_num(0); }\n");
    fprintf(f, "static Value* runtime_tostr(Value* v) { char b[64]; if(v->kind==VAL_NUM){ if(v->num==(long long)v->num) snprintf(b,64,\"%%%%lld\",(long long)v->num); else snprintf(b,64,\"%%%%g\",v->num); return val_str(b); } if(v->kind==VAL_STR) return v; return val_str(\"\"); }\n");
}

static void emit_c_expr(Node *n, FILE *f);
static void emit_c_stmt(Node *n, int indent, FILE *f);

static void emit_c_expr(Node *n, FILE *f) {
    switch (n->kind) {
        case ND_NUM:   fprintf(f, "val_num(%g)", n->num); break;
        case ND_STR:   fprintf(f, "val_str(\"%s\")", n->str); break;
        case ND_VAR:   fprintf(f, "table_get(env, \"%s\")", n->name); break;
        case ND_ARG:   fprintf(f, "io_args"); break;
        case ND_BINOP:
            fprintf(f, "runtime_binop(%d, ", n->op);
            emit_c_expr(n->left, f);
            fprintf(f, ", "); 
            emit_c_expr(n->right, f);
            fprintf(f, ")");
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
                fprintf(f, "table_set(env, \"%s\", ", n->left->name);
                emit_c_expr(n->right, f);
                fprintf(f, ")");
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
        default: fprintf(f, "val_none()"); break;
    }
}

static void emit_c_stmt(Node *n, int indent, FILE *f) {
    for (int i = 0; i < indent; i++) fprintf(f, "  ");
    switch (n->kind) {
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
        case ND_PUT:
            fprintf(f, "runtime_put(");
            emit_c_expr(n->left, f);
            fprintf(f, ", ");
            emit_c_expr(n->right, f);
            fprintf(f, ");\n");
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
        fprintf(f, "static Value* io_func_%s(Table* parent, int argc, Value** argv) {\n", n->name);
        fprintf(f, "  Table* env = table_new(parent);\n");
        /* Bind parameters */
        for (int i = 0; i < n->body_count; i++) {
            fprintf(f, "  if(argc > %d) table_set(env, \"%s\", argv[%d]);\n", i, n->body[i]->name, i);
        }
        /* Define internal functions if any */
        emit_c_functions(n->left, f);
        /* Emit body statements */
        emit_c_stmt(n->left, 1, f);
        fprintf(f, "  return val_none();\n}\n\n");
    }
}

static void compile_to_bin(Node *prog, const char *out_name, bool keep_c) {
    char c_file[512];
    snprintf(c_file, sizeof(c_file), "%s_gen.c", out_name);
    FILE *f = fopen(c_file, "w");
    if (!f) { fprintf(stderr, "Failed to create C source\n"); exit(1); }

    emit_c_header(f);
    emit_c_functions(prog, f);

    fprintf(f, "\nint main(int argc, char **argv) {\n");
    fprintf(f, "  Value* io_args = val_list();\n");
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
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "gcc -O2 %s -o %s", c_file, out_name);
#else
    snprintf(cmd, sizeof(cmd), "gcc -O2 %s -o %s", c_file, out_name);
#endif

    printf("Compiling %s...\n", out_name);
    if (system(cmd) != 0) {
        fprintf(stderr, COL_RED "Build failed." COL_RST " Ensure gcc is in your PATH.\n");
        exit(1);
    }
    printf(COL_GRN "Successfully built %s" COL_RST "\n", out_name);
    if (!keep_c) remove(c_file);
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
    enable_ansi_colors();
    const char *input_path = NULL;
    const char *output_name = NULL;
    bool keep_c = false;

    if (argc < 2) goto usage;

    /* Argument Parsing */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) output_name = argv[++i];
            else { fprintf(stderr, "Error: -o requires a filename\n"); return 1; }
        } else if (strcmp(argv[i], "-k") == 0 || strcmp(argv[i], "--keep") == 0) {
            keep_c = true;
        } else if (argv[i][0] != '-') {
            if (!input_path) input_path = argv[i];
        }
    }

    if (!input_path && strcmp(argv[1], "--version") != 0 && strcmp(argv[1], "--help") != 0) goto usage;

    if (strcmp(argv[1], "--version") == 0) {
        printf("io 1.0.0\n");
        return 0;
    }
    if (strcmp(argv[1], "--help") == 0) {
        printf("io - a minimalist programming language\n");
        printf("Usage: io <file.io> [-o program] [-k] [args...]\n");
        printf("       io --version\n");
        printf("       io --help\n\n");
        printf("Options:\n");
        printf("  -o <name>   Compile to executable\n");
        printf("  -k, --keep  Keep transpiled .c source\n\n");
        printf("The 21-word vocabulary:\n");
        printf("  Data:       Numbers, Text (\"...\")\n");
        printf("  Assignment: =\n");
        printf("  Logic:      ==\n");
        printf("  Math:       + - * / %%\n");
        printf("  Comparison: < >\n");
        printf("  Control:    if else while\n");
        printf("  Functions:  do\n");
        printf("  Memory:     list at set\n");
        printf("  I/O:        ask put\n");
        printf("  Environ:    arg paste\n");
        printf("  Conversion: len ord chr tonum tostr\n");
        printf("  Comments:   #\n");
        return 0;
    }

    /* Windows .exe handling */
#ifdef _WIN32
    char win_out[512];
    if (output_name && !strstr(output_name, ".exe") && !strstr(output_name, ".EXE")) {
        snprintf(win_out, sizeof(win_out), "%s.exe", output_name);
        output_name = win_out;
    }
#endif

    /* build CLI arg list */
    cli_args = val_list();
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) { i++; continue; }
        if (strcmp(argv[i], "-k") == 0 || strcmp(argv[i], "--keep") == 0) continue;
        if (argv[i] == input_path) continue;
        
        if (cli_args->item_count >= cli_args->item_cap) {
            cli_args->item_cap = cli_args->item_cap ? cli_args->item_cap * 2 : 8;
            cli_args->items = realloc(cli_args->items, cli_args->item_cap * sizeof(Value*));
        }
        cli_args->items[cli_args->item_count++] = val_str(argv[i]);
    }

    char *src = read_file(input_path);
    Lexer lexer = { .src = src, .pos = 0, .filename = input_path };
    lex(&lexer, input_path);

    Parser parser = { .tokens = lexer.tokens, .pos = 0, .src = src, .filename = input_path };
    Node *prog = parse_program(&parser);

    if (output_name) {
        compile_to_bin(prog, output_name, keep_c);
        free(src);
        return 0;
    }

    /* Otherwise, Interpret */
    Table *global = table_new(NULL);
    eval(prog, global);

    free(src);
    return 0;

usage:
    fprintf(stderr, "Usage: io <file.io> [-o program] [args...]\n");
    return 1;
}
