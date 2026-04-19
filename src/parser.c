#include "vii.h"

Node *nd_new(NdKind kind) {
    Node *n = calloc(1, sizeof(Node));
    n->kind = kind;
    return n;
}

void nd_push(Node *block, Node *child) {
    if (block->body_count >= block->body_cap) {
        block->body_cap = block->body_cap ? block->body_cap * 2 : 8;
        block->body = realloc(block->body, block->body_cap * sizeof(Node*));
    }
    block->body[block->body_count++] = child;
}

/* ──────────────────────── Parser helpers ──────────────────────── */

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
static Node *parse_block(Parser *p, bool is_function);

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
        case TOK_SYS:   advance(p); { Node *n = nd_new(ND_SYS);   n->left = parse_postfix(p); return n; }
        case TOK_ENV:   advance(p); { Node *n = nd_new(ND_ENV);   n->left = parse_postfix(p); return n; }
        case TOK_EXIT:  advance(p); { Node *n = nd_new(ND_EXIT);  n->left = parse_postfix(p); return n; }
        case TOK_LIST:  advance(p); return nd_new(ND_LIST);
        case TOK_DO: {
            advance(p);
            Node *fn = nd_new(ND_DO);
            fn->name = strdup(advance(p)->text);
            fn->body_cap = 8;
            fn->body = calloc(fn->body_cap, sizeof(Node*));
            while (peek(p)->kind == TOK_IDENT) {
                Node *param = nd_new(ND_VAR);
                param->name = strdup(advance(p)->text);
                if (peek(p)->kind == TOK_ARROW) {
                    advance(p);
                    if (peek(p)->kind == TOK_IDENT)
                        param->type_tag = strdup(advance(p)->text);
                }
                nd_push(fn, param);
            }
            skip_newlines(p);
            expect(p, TOK_INDENT);
            fn->left = parse_block(p, true);
            expect(p, TOK_DEDENT);
            return fn;
        }
        default:
            report_error(p->filename, p->src, t->pos, t->line, "unexpected token '%s'", t->text ? t->text : "");
    }
    return NULL;
}

static Node *parse_postfix(Parser *p) {
    Node *expr = parse_primary(p);

    while (true) {
        if (peek(p)->kind == TOK_AT) {
            advance(p);
            Node *at = nd_new(ND_AT);
            at->left = expr;
            at->right = parse_postfix(p);
            expr = at;
        } else if (peek(p)->kind == TOK_SET) {
            advance(p);
            Node *set = nd_new(ND_SET);
            set->left = expr;
            set->body_cap = 4;
            set->body = calloc(set->body_cap, sizeof(Node*));
            nd_push(set, parse_postfix(p));
            nd_push(set, parse_expr(p));
            expr = set;
        } else if (peek(p)->kind == TOK_PUT) {
            advance(p);
            Node *put = nd_new(ND_PUT);
            put->left = expr;
            put->right = parse_expr(p);
            expr = put;
        } else if (peek(p)->kind == TOK_ASK && expr->kind != ND_ASK) {
            advance(p);
            Node *af = nd_new(ND_ASKFILE);
            af->left = expr;
            expr = af;
        } else if (peek(p)->kind == TOK_IDENT || peek(p)->kind == TOK_NUM ||
                   peek(p)->kind == TOK_STR || peek(p)->kind == TOK_ASK ||
                   peek(p)->kind == TOK_LIST || peek(p)->kind == TOK_ARG ||
                   peek(p)->kind == TOK_LEN || peek(p)->kind == TOK_ORD ||
                   peek(p)->kind == TOK_CHR || peek(p)->kind == TOK_TONUM ||
                   peek(p)->kind == TOK_TOSTR || peek(p)->kind == TOK_SYS ||
                   peek(p)->kind == TOK_ENV || peek(p)->kind == TOK_EXIT) {
            if (expr->kind != ND_VAR) break;

            if (peek(p)->kind == TOK_IDENT && (p->tokens[p->pos+1].kind == TOK_EQ || p->tokens[p->pos+1].kind == TOK_ARROW)) {
                break;
            }

            Node *call = nd_new(ND_CALL);
            call->left = expr;
            call->body_cap = 8;
            call->body = calloc(call->body_cap, sizeof(Node*));
            while (peek(p)->kind == TOK_IDENT || peek(p)->kind == TOK_NUM ||
                   peek(p)->kind == TOK_STR || peek(p)->kind == TOK_ASK ||
                   peek(p)->kind == TOK_LIST || peek(p)->kind == TOK_ARG ||
                   peek(p)->kind == TOK_LEN || peek(p)->kind == TOK_ORD ||
                   peek(p)->kind == TOK_CHR || peek(p)->kind == TOK_TONUM ||
                   peek(p)->kind == TOK_TOSTR || peek(p)->kind == TOK_SYS ||
                   peek(p)->kind == TOK_ENV || peek(p)->kind == TOK_EXIT) {
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

    /* assignment: var = expr OR var type = expr */
    bool is_typed = (left->kind == ND_VAR && peek(p)->kind == TOK_IDENT &&
                    (p->tokens[p->pos+1].kind == TOK_EQ || p->tokens[p->pos+1].kind == TOK_ARROW));
    bool is_normal = (peek(p)->kind == TOK_EQ && (left->kind == ND_VAR || left->kind == ND_AT));

    if (is_typed || is_normal) {
        Node *assign = nd_new(ND_ASSIGN);
        assign->left = left;

        if (is_typed) {
            left->type_tag = strdup(advance(p)->text);
            advance(p);
        } else {
            advance(p);
        }

        assign->right = parse_expr(p);
        return assign;
    }

    return left;
}

static Node *parse_stmt(Parser *p) {
    skip_newlines(p);
    Token *t = peek(p);

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
        n->left = parse_expr(p);
        skip_newlines(p);
        expect(p, TOK_INDENT);
        n->body = NULL; n->body_count = 0; n->body_cap = 0;
        nd_push(n, parse_block(p, false));
        expect(p, TOK_DEDENT);
        skip_newlines(p);
        if (peek(p)->kind == TOK_ELSE) {
            advance(p);
            skip_newlines(p);
            if (peek(p)->kind == TOK_IF) {
                n->right = parse_stmt(p);
            } else {
                expect(p, TOK_INDENT);
                n->right = parse_block(p, false);
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
        nd_push(n, parse_block(p, false));
        expect(p, TOK_DEDENT);
        return n;
    }

    /* expression statement (with implicit print) */
    Node *expr = parse_expr(p);
    if (expr->kind != ND_ASSIGN && expr->kind != ND_DO && expr->kind != ND_SET && expr->kind != ND_PUT && expr->kind != ND_EXIT) {
        Node *print = nd_new(ND_PRINT);
        print->left = expr;
        return print;
    }
    return expr;
}

static Node *parse_block(Parser *p, bool is_function) {
    Node *block = nd_new(ND_BLOCK);
    while (peek(p)->kind != TOK_DEDENT && peek(p)->kind != TOK_EOF) {
        if (peek(p)->kind == TOK_NEWLINE) { advance(p); continue; }

        Node *stmt = parse_stmt(p);
        skip_newlines(p);

        if (is_function && (peek(p)->kind == TOK_DEDENT || peek(p)->kind == TOK_EOF)) {
            if (stmt->kind == ND_PRINT) {
                Node *expr = stmt->left;
                free(stmt);
                stmt = expr;
            }
        }
        nd_push(block, stmt);
        skip_newlines(p);
    }
    return block;
}

Node *parse_program(Parser *p) {
    Node *prog = nd_new(ND_BLOCK);
    while (peek(p)->kind != TOK_EOF) {
        if (peek(p)->kind == TOK_NEWLINE) { advance(p); continue; }
        nd_push(prog, parse_stmt(p));
        skip_newlines(p);
    }
    return prog;
}
