#include "vii.h"

/* Circular paste detection */
#define MAX_PASTE_DEPTH 32
static const char *paste_stack[MAX_PASTE_DEPTH];
static int paste_depth = 0; 

/* Read file contents */
static char* read_file_contents(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

/* Check if file already in paste stack (circular check) */
static int is_paste_cycle(const char *filename) {
    for (int i = 0; i < paste_depth; i++) {
        if (strcmp(paste_stack[i], filename) == 0) return 1;
    }
    return 0;
}

/* Check for #! ONCE at start of file */
static int has_shebang_once(const char *src) {
    if (strncmp(src, "#! ONCE", 7) == 0) return 1;
    return 0;
}

/* Set to track already-pasted ONCE files */
#define MAX_ONCE_FILES 64
static const char *once_files[MAX_ONCE_FILES];
static int once_count = 0;

static int was_once_pasted(const char *filename) {
    for (int i = 0; i < once_count; i++) {
        if (strcmp(once_files[i], filename) == 0) return 1;
    }
    return 0;
}

static void mark_once_pasted(const char *filename, Arena *arena) {
    if (once_count < MAX_ONCE_FILES) {
        once_files[once_count++] = arena_strdup(arena, filename);
    }
}

static const char *tok_kw[] = {
    "if","else","while","break","do","ask","list","dict","key","keys","at","set","put","arg","paste","len","ord","chr","tonum","tostr","slice","type","time","append","heap_alloc","heap_free","valof","val","sizeof","sys","env","exit","ref","ptr","bit","for","in","split","trim","replace","safe","and","or","out","not","addr","stack_alloc","ent",
    /* Type system keywords */
    "i8","i16","i32","i64","u8","u16","u32","u64","f32","f64"
};
static const TokKind kw_kind[] = {
    TOK_IF,TOK_ELSE,TOK_WHILE,TOK_BREAK,TOK_DO,TOK_ASK,TOK_LIST,TOK_DICT,TOK_KEY,TOK_KEYS,TOK_AT,TOK_SET,TOK_PUT,TOK_ARG,TOK_PASTE,TOK_LEN,TOK_ORD,TOK_CHR,TOK_TONUM,TOK_TOSTR,TOK_SLICE,TOK_TYPE,TOK_TIME,TOK_APPEND,TOK_HEAP_ALLOC,TOK_HEAP_FREE,TOK_VALOF,TOK_VALOF,TOK_SIZEOF,TOK_SYS,TOK_ENV,TOK_EXIT,TOK_REF,TOK_PTR,TOK_BIT,TOK_FOR,TOK_IN,TOK_SPLIT,TOK_TRIM,TOK_REPLACE,TOK_SAFE,TOK_AND,TOK_OR,TOK_OUT,TOK_NOT,TOK_ADDR,TOK_STACK_ALLOC,TOK_ENT,
    /* Type system tokens */
    TOK_I8,TOK_I16,TOK_I32,TOK_I64,TOK_U8,TOK_U16,TOK_U32,TOK_U64,TOK_F32,TOK_F64
};
#define KW_COUNT 58

static void lex_push(Lexer *l, Token t) {
    if (l->tok_count >= l->tok_cap) {
        int old_cap = l->tok_cap;
        l->tok_cap = l->tok_cap ? l->tok_cap * 2 : 256;
        Token *new_tokens = arena_alloc(l->arena, l->tok_cap * sizeof(Token));
        if (l->tokens) memcpy(new_tokens, l->tokens, old_cap * sizeof(Token));
        l->tokens = new_tokens;
    }
    l->tokens[l->tok_count++] = t;
}

/* Helper to simplify allocation in lexer */
static char* lex_alloc_text(Lexer *l, int len) {
    // Assuming l->arena is initialized in main or lex()
    return arena_alloc(l->arena, len + 1);
}

/* Forward declarations */
static void lex_token(Lexer *l);
static void lex_paste_inline(Lexer *l, const char *filename);

/* Recursively lex a pasted file and inline its tokens */
static void lex_paste_inline(Lexer *l, const char *filename) {
    extern int trace;
    if (trace) fprintf(stderr, "[LEXER] Pasting file: %s\n", filename);
    
    /* Check for circular paste */
    if (is_paste_cycle(filename)) {
        report_error(l->filename, l->src, l->pos, l->line, "circular paste detected: %s", filename);
        return;
    }

    /* Check depth limit */
    if (paste_depth >= MAX_PASTE_DEPTH) {
        report_error(l->filename, l->src, l->pos, l->line, "paste depth exceeded limit");
        return;
    }

    /* Read file */
    char *src = read_file_contents(filename);
    if (!src) {
        report_error(l->filename, l->src, l->pos, l->line, "cannot read paste file: %s", filename);
        return;
    }
    if (trace) fprintf(stderr, "[LEXER] Read %lu bytes from %s\n", (unsigned long)strlen(src), filename);

    /* Check for #! ONCE guard */
    if (has_shebang_once(src)) {
        if (was_once_pasted(filename)) {
            free(src);
            return; /* Already pasted, skip */
        }
        mark_once_pasted(filename, l->arena);
    }

    /* Push to paste stack for cycle detection */
    paste_stack[paste_depth++] = arena_strdup(l->arena, filename);

    /* Create sub-lexer with same arena but different source */
    Lexer sub = {0};
    sub.arena = l->arena;
    sub.src = src;
    sub.pos = 0;
    sub.line = 1;
    sub.filename = arena_strdup(l->arena, filename);
    sub.tok_count = 0;
    sub.tok_cap = 0;
    sub.tokens = NULL;
    sub.indent = 0;
    sub.indent_top = 0;
    sub.indents[0] = 0;
    sub.at_line_start = true;

    /* Lex the sub-file by calling lex() */
    lex(&sub, filename);
    if (trace) fprintf(stderr, "[LEXER] Sub-file %s produced %d tokens\n", filename, sub.tok_count);

    /* Copy sub-tokens to parent (excluding the final EOF) */
    int tokens_added = 0;
    for (int i = 0; i < sub.tok_count - 1; i++) {
        lex_push(l, sub.tokens[i]);
        tokens_added++;
    }
    if (trace) fprintf(stderr, "[LEXER] Added %d tokens from %s to parent\n", tokens_added, filename);

    /* Cleanup */
    free(src);
    paste_depth--;
}

/* Main entry point */
void lex(Lexer *l, const char *filename) {
    l->tok_count = 0;
    l->indent = 0;
    l->indent_top = 0;
    l->indents[0] = 0;
    l->at_line_start = true;
    l->line = 1;
    l->filename = filename;

    while (l->src[l->pos]) {
        lex_token(l);
    }

    /* emit remaining dedents */
    while (l->indent > 0) {
        l->indent_top--;
        l->indent = l->indents[l->indent_top];
        lex_push(l, (Token){TOK_DEDENT, NULL, 0, l->line, l->pos});
    }
    lex_push(l, (Token){TOK_EOF, NULL, 0, l->line, l->pos});
}

/* Process one token */
static void lex_token(Lexer *l) {
    char c = l->src[l->pos];

    /* skip \r (CRLF) */
    if (c == '\r') { l->pos++; return; }

    /* newline */
    if (c == '\n') {
        if (l->tok_count == 0 || l->tokens[l->tok_count-1].kind != TOK_NEWLINE)
            lex_push(l, (Token){TOK_NEWLINE, arena_strdup(l->arena, "\\n"), 0, l->line, l->pos});
        l->line++;
        l->pos++;
        l->at_line_start = true;
        return;
    }

    /* indent/dedent at line start */
    if (l->at_line_start) {
        int spaces = 0;
        int p = l->pos;
        while (l->src[p] == ' ') { spaces++; p++; }

        char next = l->src[p];
        if (next == '\n' || next == '\r' || next == '#' || next == '\0') {
            l->pos = p;
            l->at_line_start = false;
            return;
        } else {
            if (spaces > l->indent) {
                l->indents[++l->indent_top] = spaces;
                l->indent = spaces;
                lex_push(l, (Token){TOK_INDENT, arena_strdup(l->arena, "<indent>"), 0, l->line, l->pos});
            }
            while (spaces < l->indent) {
                l->indent_top--;
                l->indent = l->indents[l->indent_top];
                lex_push(l, (Token){TOK_DEDENT, arena_strdup(l->arena, "<dedent>"), 0, l->line, l->pos});
            }
            l->pos = p;
            l->at_line_start = false;
        }
        c = l->src[l->pos];
        if (!c) return;
    }

    /* skip spaces */
    if (c == ' ' || c == '\t') { l->pos++; return; }

    /* shebang or comment */
    if (c == '#') {
        /* Check for #! ONCE shebang at very beginning of file */
        if (l->pos == 0 && l->src[1] == '!' && strncmp(l->src + 2, " ONCE", 5) == 0) {
            lex_push(l, (Token){TOK_SHEBANG, arena_strdup(l->arena, "#! ONCE"), 0, l->line, 0});
            l->pos += 7; /* skip past #! ONCE */
            return;
        }
        while (l->src[l->pos] && l->src[l->pos] != '\n' && l->src[l->pos] != '\r') l->pos++;
        return;
    }

    /* number */
    if (isdigit(c)) {
        int start = l->pos;
        while (isdigit(l->src[l->pos])) l->pos++;
        if (l->src[l->pos] == '.') { l->pos++; while (isdigit(l->src[l->pos])) l->pos++; }
        char *text = lex_alloc_text(l, l->pos - start);
        memcpy(text, l->src + start, l->pos - start);
        text[l->pos - start] = '\0';
        double num = atof(text);
        lex_push(l, (Token){TOK_NUM, text, num, l->line, start});
        return;
    }

    /* string */
    if (c == '"') {
        l->pos++;
        int start = l->pos;
        while (l->src[l->pos] && l->src[l->pos] != '"') {
            if (l->src[l->pos] == '\\') l->pos++;
            l->pos++;
        }
        char *text = lex_alloc_text(l, l->pos - start);
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
        return;
    }

    /* operators */
    if (c == '=' && l->src[l->pos+1] == '=') {
        lex_push(l, (Token){TOK_EQEQ, arena_strdup(l->arena, "=="), 0, l->line, l->pos});
        l->pos += 2; return;
    }
    if (c == '-' && l->src[l->pos+1] == '>') {
        lex_push(l, (Token){TOK_ARROW, arena_strdup(l->arena, "->"), 0, l->line, l->pos});
        l->pos += 2; return;
    }
    if (c == '<' && l->src[l->pos+1] == '=') {
        lex_push(l, (Token){TOK_LTE, arena_strdup(l->arena, "<="), 0, l->line, l->pos});
        l->pos += 2; return;
    }
    if (c == '>' && l->src[l->pos+1] == '=') {
        lex_push(l, (Token){TOK_GTE, arena_strdup(l->arena, ">="), 0, l->line, l->pos});
        l->pos += 2; return;
    }
    if (c == '!' && l->src[l->pos+1] == '=') {
        lex_push(l, (Token){TOK_NE, arena_strdup(l->arena, "!="), 0, l->line, l->pos});
        l->pos += 2; return;
    }
    if (c == '.' && l->src[l->pos+1] == '.') {
        lex_push(l, (Token){TOK_DOTDOT, arena_strdup(l->arena, ".."), 0, l->line, l->pos});
        l->pos += 2; return;
    }
    if (c == '=') { lex_push(l, (Token){TOK_EQ, arena_strdup(l->arena, "="), 0, l->line, l->pos}); l->pos++; return; }
    if (c == '+') { lex_push(l, (Token){TOK_PLUS, arena_strdup(l->arena, "+"), 0, l->line, l->pos}); l->pos++; return; }
    if (c == '-') { lex_push(l, (Token){TOK_MINUS, arena_strdup(l->arena, "-"), 0, l->line, l->pos}); l->pos++; return; }
    if (c == '*') { lex_push(l, (Token){TOK_STAR, arena_strdup(l->arena, "*"), 0, l->line, l->pos}); l->pos++; return; }
    if (c == '/') { lex_push(l, (Token){TOK_SLASH, arena_strdup(l->arena, "/"), 0, l->line, l->pos}); l->pos++; return; }
    if (c == '%') { lex_push(l, (Token){TOK_PCT, arena_strdup(l->arena, "%"), 0, l->line, l->pos}); l->pos++; return; }
    if (c == '<') { lex_push(l, (Token){TOK_LT, arena_strdup(l->arena, "<"), 0, l->line, l->pos}); l->pos++; return; }
    if (c == '>') { lex_push(l, (Token){TOK_GT, arena_strdup(l->arena, ">"), 0, l->line, l->pos}); l->pos++; return; }

    /* parentheses */
    if (c == '(') { lex_push(l, (Token){TOK_LPAREN, arena_strdup(l->arena, "("), 0, l->line, l->pos}); l->pos++; return; }
    if (c == ')') { lex_push(l, (Token){TOK_RPAREN, arena_strdup(l->arena, ")"), 0, l->line, l->pos}); l->pos++; return; }
    /* brackets for array syntax */
    if (c == '[') { lex_push(l, (Token){TOK_LBRACKET, arena_strdup(l->arena, "["), 0, l->line, l->pos}); l->pos++; return; }
    if (c == ']') { lex_push(l, (Token){TOK_RBRACKET, arena_strdup(l->arena, "]"), 0, l->line, l->pos}); l->pos++; return; }
    if (c == ';') { lex_push(l, (Token){TOK_SEMICOLON, arena_strdup(l->arena, ";"), 0, l->line, l->pos}); l->pos++; return; }

    /* identifier / keyword - with special paste handling */
    if (isalpha(c) || c == '_') {
        int start = l->pos;
        while (isalnum(l->src[l->pos]) || l->src[l->pos] == '_') l->pos++;
        int len = l->pos - start;
        char *text = lex_alloc_text(l, len);
        memcpy(text, l->src + start, len);
        text[len] = '\0';
        TokKind kind = TOK_IDENT;
        for (int i = 0; i < KW_COUNT; i++) {
            if (strcmp(text, tok_kw[i]) == 0) { kind = kw_kind[i]; break; }
        }

        /* Inline paste handling: paste "filename" */
        if (kind == TOK_PASTE) {
            /* Skip whitespace and optional newline */
            int p = l->pos;
            while (l->src[p] == ' ' || l->src[p] == '\t') p++;
            /* Allow newline before string in paste statement */
            if (l->src[p] == '\n') {
                p++;
                while (l->src[p] == ' ' || l->src[p] == '\t') p++;
            }
            /* Expect string */
            if (l->src[p] == '"') {
                p++; /* skip opening quote */
                int fn_start = p;
                while (l->src[p] && l->src[p] != '"') {
                    if (l->src[p] == '\\' && l->src[p+1]) p += 2;
                    else p++;
                }
                int fn_len = p - fn_start;
                char *filename = malloc(fn_len + 1);
                memcpy(filename, l->src + fn_start, fn_len);
                filename[fn_len] = '\0';

                /* Inline the pasted file's tokens */
                lex_paste_inline(l, filename);

                free(filename);
                if (l->src[p] == '"') p++; /* skip closing quote */
                l->pos = p;
                return;
            } else {
                report_error(l->filename, l->src, p, l->line, "expected string filename after 'paste'");
            }
        }

        lex_push(l, (Token){kind, text, 0, l->line, start});
        return;
    }

    report_error(l->filename, l->src, l->pos, l->line, "unexpected character '%c'", c);
}
