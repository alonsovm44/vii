#ifndef VII_H
#define VII_H

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

void enable_ansi_colors(void);
void report_error(const char *filename, const char *src, int pos, int line, const char *fmt, ...);
char *read_file(const char *path);

/* ──────────────────────── Arena ────────────────────────── */

typedef struct {
    char  *data;
    size_t capacity;
    size_t offset;
} Arena;

Arena* arena_create(size_t size);
void*  arena_alloc(Arena *a, size_t size);
char*  arena_strdup(Arena *a, const char *s);
void   arena_reset(Arena *a);

extern Arena *global_arena;

/* ──────────────────────── Value ──────────────────────── */

typedef enum { VAL_NUM, VAL_STR, VAL_LIST, VAL_DICT, VAL_BIT, VAL_REF, VAL_BREAK, VAL_OUT, VAL_NONE } ValKind;

typedef struct Value {
    ValKind kind;
    double num;
    char  *str;
    struct Value **items;
    struct Value *target; /* for VAL_REF */
    int    item_count;
    int    item_cap;
    struct Table *fields;
    struct Value *inner; /* for VAL_OUT */
} Value;

Value *val_num(double n);
Value *val_str(const char *s);
Value *val_list(void);
Value *val_dict(void);
Value *val_bit(bool b);
Value *val_ref(Value *target);
Value *val_break(void);
Value *val_none(void);
void   val_list_grow(Value *v);
bool   val_truthy(Value *v);
void   val_print_to(Value *v, FILE *f);
void   val_print(Value *v);

/* ──────────────────────── ALL_CAPS helper ──────────────────────── */

static inline bool is_all_caps(const char *s) {
    if (!s || !*s) return false;
    bool has_upper = false;
    for (const char *c = s; *c; c++) {
        if (*c >= 'a' && *c <= 'z') return false;
        if (*c >= 'A' && *c <= 'Z') has_upper = true;
    }
    return has_upper;
}

/* ──────────────────────── Lexer ──────────────────────── */

typedef enum {
    TOK_NUM, TOK_STR, TOK_IDENT,
    TOK_IF, TOK_ELSE, TOK_WHILE, TOK_BREAK, TOK_DO, TOK_ASK, TOK_LIST, TOK_DICT, TOK_KEY, TOK_KEYS, TOK_AT, TOK_SET,
    TOK_PUT, TOK_ARG, TOK_PASTE, TOK_LEN, TOK_ORD, TOK_CHR, TOK_TONUM, TOK_TOSTR, TOK_SLICE, TOK_TYPE, TOK_TIME, TOK_APPEND,
    TOK_SYS, TOK_ENV, TOK_EXIT, TOK_REF, TOK_PTR, TOK_BIT, TOK_SPLIT, TOK_TRIM, TOK_REPLACE, TOK_SAFE,
    TOK_FOR, TOK_IN, TOK_EQ, TOK_EQEQ, TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PCT, TOK_ARROW,
    TOK_LT, TOK_GT, TOK_LTE, TOK_GTE, TOK_NE, TOK_AND, TOK_OR, 
    TOK_LPAREN, TOK_RPAREN, TOK_SEMICOLON,
    TOK_NEWLINE, TOK_INDENT, TOK_DEDENT, TOK_EOF, TOK_OUT, TOK_NOT
} TokKind;

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
    int         indent;
    int         indents[256];
    int         indent_top;
    bool        at_line_start;
    Token      *tokens;
    int         tok_count;
    int         tok_cap;
    Arena      *arena;
} Lexer;

void lex(Lexer *l, const char *filename);

/* ──────────────────────── AST ──────────────────────── */

typedef enum {
    ND_NUM, ND_STR, ND_VAR,
    ND_ASSIGN, ND_BINOP, ND_UMINUS,
    ND_IF, ND_WHILE, ND_BREAK, ND_DO,
    ND_FOR, ND_SAFE, ND_ASK, ND_ASKFILE, ND_LIST, ND_DICT, ND_KEY, ND_KEYS, ND_AT, ND_SET,
    ND_PUT, ND_ARG, ND_LEN, ND_ORD, ND_CHR, ND_TONUM, ND_TOSTR, ND_SLICE, ND_SPLIT, ND_TRIM, ND_REPLACE, ND_TYPE, ND_TIME,
    ND_SYS, ND_ENV, ND_EXIT, ND_REF, ND_OUT, ND_NOT,
    ND_CALL, ND_BLOCK,
    ND_PRINT
} NdKind;

typedef struct Node {
    NdKind kind;
    char  *name;
    char  *type_tag;
    char  *mod_tag;
    double num;
    char  *str;
    TokKind op;
    struct Node *left;
    struct Node *right;
    struct Node **body;
    int    body_count;
    int    body_cap;
} Node;

Node *nd_new(NdKind kind);
void  nd_push(Node *block, Node *child);

/* ──────────────────────── Parser ──────────────────────── */

typedef struct Parser {
    Token *tokens;
    int    pos;
    const char *filename;
    const char *src;
    int    in_func;
    Node  *current_func;
    Arena      *arena;
} Parser;

Node *parse_program(Parser *p);

/* ──────────────────────── Interpreter ──────────────────────── */

typedef struct Entry {
    char       *key;
    Value      *val;
    bool        is_constant;
    struct Entry *next;
} Entry;

#define TABLE_SIZE 256

typedef struct Table {
    Entry      *buckets[TABLE_SIZE];
    struct Table *parent;
} Table;

typedef struct Func {
    char  *name;
    Node  *def;
    struct Func *next;
} Func;

Table *table_new(Table *parent);
Value *table_get(Table *t, const char *key);
void   table_set(Table *t, const char *key, Value *val);

Value *eval(Node *n, Table *env);
Value *eval_block(Node *block, Table *env);

/* global CLI args (set in main) */
extern Value *cli_args;

/* global CLI defines for IF macros (set in main) */
extern char **cli_defines;
extern int    cli_define_count;

/* ──────────────────────── Codegen ──────────────────────── */

void compile_to_bin(Node *prog, const char *out_name, bool keep_c);

/* ──────────────────────── Debug ──────────────────────── */

void dump_ast_json(Node *n, FILE *f, int indent);

#endif /* VII_H */
