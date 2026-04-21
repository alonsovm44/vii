#include "vii.h"

static const char *tok_kw[] = {
    "if","else","while","break","do","ask","list","dict","key","at","set","put","arg","paste","len","ord","chr","tonum","tostr","slice","type","time","append","sys","env","exit","ref","ptr","bit","and","or"
};
static const TokKind kw_kind[] = {
    TOK_IF,TOK_ELSE,TOK_WHILE,TOK_BREAK,TOK_DO,TOK_ASK,TOK_LIST,TOK_DICT,TOK_KEY,TOK_AT,TOK_SET,TOK_PUT,TOK_ARG,TOK_PASTE,TOK_LEN,TOK_ORD,TOK_CHR,TOK_TONUM,TOK_TOSTR,TOK_SLICE,TOK_TYPE,TOK_TIME,TOK_APPEND,TOK_SYS,TOK_ENV,TOK_EXIT,TOK_REF,TOK_PTR,TOK_BIT,TOK_AND,TOK_OR
};
#define KW_COUNT 31

static void lex_push(Lexer *l, Token t) {
    if (l->tok_count >= l->tok_cap) {
        l->tok_cap = l->tok_cap ? l->tok_cap * 2 : 256;
        l->tokens = realloc(l->tokens, l->tok_cap * sizeof(Token));
    }
    l->tokens[l->tok_count++] = t;
}

void lex(Lexer *l, const char *filename) {
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
            if (l->tok_count == 0 || l->tokens[l->tok_count-1].kind != TOK_NEWLINE)
                lex_push(l, (Token){TOK_NEWLINE, NULL, 0, l->line, l->pos});
            l->line++;
            l->pos++;
            l->at_line_start = true;
            continue;
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
                continue;
            } else {
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
                l->pos = p;
                l->at_line_start = false;
            }
            c = l->src[l->pos];
            if (!c) break;
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
        if (c == '-' && l->src[l->pos+1] == '>') {
            lex_push(l, (Token){TOK_ARROW, strdup("->"), 0, l->line, l->pos});
            l->pos += 2; continue;
        }
        if (c == '<' && l->src[l->pos+1] == '=') {
            lex_push(l, (Token){TOK_LTE, strdup("<="), 0, l->line, l->pos});
            l->pos += 2; continue;
        }
        if (c == '>' && l->src[l->pos+1] == '=') {
            lex_push(l, (Token){TOK_GTE, strdup(">="), 0, l->line, l->pos});
            l->pos += 2; continue;
        }
        if (c == '!' && l->src[l->pos+1] == '=') {
            lex_push(l, (Token){TOK_NE, strdup("!="), 0, l->line, l->pos});
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

        /* parentheses */
        if (c == '(') { lex_push(l, (Token){TOK_LPAREN, strdup("("), 0, l->line, l->pos}); l->pos++; continue; }
        if (c == ')') { lex_push(l, (Token){TOK_RPAREN, strdup(")"), 0, l->line, l->pos}); l->pos++; continue; }

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
