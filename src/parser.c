#include "vii.h"
#include <ctype.h>

static int parser_current_line = 0;
static const char *parser_current_filename = NULL;

Node *nd_new(NdKind kind) {
    Node *n = arena_alloc(global_arena, sizeof(Node));
    n->kind = kind;
    n->line = parser_current_line; // Set the line number here
    n->filename = (char*)parser_current_filename;
    return n;
}

void nd_push(Node *block, Node *child) {
    if (block->body_count >= block->body_cap) {
        int old_cap = block->body_cap;
        block->body_cap = block->body_cap ? block->body_cap * 2 : 8;
        Node **new_body = arena_alloc(global_arena, block->body_cap * sizeof(Node*));
        if (block->body) {
            memcpy(new_body, block->body, old_cap * sizeof(Node*));
        }
        block->body = new_body;
    }
    block->body[block->body_count++] = child;
}

static Node *strip_prints(Node *n) {
    if (!n) return NULL;
    if (n->kind == ND_PRINT) return n->left;
    if (n->kind == ND_BLOCK && n->body_count > 0) {
        n->body[n->body_count - 1] = strip_prints(n->body[n->body_count - 1]);
    }
    return n;
}

/* ──────────────────────── Constant Tracking ──────────────────────── */

static char **parsed_constants = NULL;
static int parsed_const_count = 0;
static int parsed_const_cap = 0;

static void track_constant(Parser *p, const char *name, int pos, int line) {
    for (int i = 0; i < parsed_const_count; i++) {
        if (strcmp(parsed_constants[i], name) == 0) {
            report_error(p->filename, p->src, pos, line, "cannot reassign constant '%s'", name);
            fflush(stderr);
            exit(1);
        }
    }
    if (parsed_const_count >= parsed_const_cap) {
        int old_cap = parsed_const_cap;
        parsed_const_cap = parsed_const_cap ? parsed_const_cap * 2 : 8;
        char **new_constants = arena_alloc(global_arena, parsed_const_cap * sizeof(char*));
        if (parsed_constants) memcpy(new_constants, parsed_constants, old_cap * sizeof(char*));
        parsed_constants = new_constants;
    }
    parsed_constants[parsed_const_count++] = arena_intern(global_arena, name);
}

/* ──────────────────────── Parser helpers ──────────────────────── */

static Token *peek(Parser *p) { 
    if (p->tokens[p->pos].line > 0) parser_current_line = p->tokens[p->pos].line;
    return &p->tokens[p->pos]; 
}
static Token *advance(Parser *p) { 
    Token *t = &p->tokens[p->pos++];
    parser_current_line = t->line;
    return t;
}

static void expect(Parser *p, TokKind k) {
    if (peek(p)->kind != k) {
        report_error(p->filename, p->src, peek(p)->pos, peek(p)->line, "expected token kind %d, got %d", k, peek(p)->kind);
    }
    advance(p);
}

static void skip_newlines(Parser *p) {
    while (peek(p)->kind == TOK_NEWLINE || peek(p)->kind == TOK_SEMICOLON) advance(p);
}

/* forward declarations */
static Node *parse_postfix(Parser *p);
static Node *parse_expr(Parser *p);
static Node *parse_block(Parser *p, bool is_function);

/* Infers the type of a node at compile-time for validation */
static const char *infer_node_type(Node *n, Node *fn_ctx) {
    if (!n) return "none";
    switch (n->kind) {
        case ND_NUM: return "num";
        case ND_STR: return "str";
        case ND_VAR:
            if (n->type_tag) return n->type_tag;
            /* Check if it matches a typed parameter in the current function context */
            if (fn_ctx) {
                for (int i = 0; i < fn_ctx->body_count; i++) {
                    if (strcmp(fn_ctx->body[i]->name, n->name) == 0) {
                        return fn_ctx->body[i]->type_tag ? fn_ctx->body[i]->type_tag : "unknown";
                    }
                }
            }
            return "unknown";
        case ND_BINOP:
            if (n->op == TOK_EQEQ || n->op == TOK_NE || n->op == TOK_LT || n->op == TOK_GT ||
                n->op == TOK_LTE || n->op == TOK_GTE || n->op == TOK_AND || n->op == TOK_OR)
                return "bit";
            return "num";
        case ND_NOT: return "bit";
        case ND_BLOCK:
            if (n->body_count > 0) return infer_node_type(n->body[n->body_count - 1], fn_ctx);
            break;
        case ND_LEN: case ND_ORD: case ND_TONUM: case ND_SYS: case ND_TIME: return "num";
        case ND_CHR: case ND_TOSTR: case ND_ASKFILE: case ND_ENV: return "str";
        case ND_LIST: return "list";
        case ND_DICT: return "dict";
        case ND_SPLIT: return "list";
        case ND_TRIM: return "str";
        case ND_REF: return "ptr";
        case ND_CALL: return "unknown"; /* Function return types are resolved at runtime or in a later pass */
        default: break;
    }
    return "unknown";
}

static Node *parse_primary(Parser *p) {
    Token *t = peek(p);
    switch (t->kind) {
        case TOK_LPAREN: {
            advance(p);
            Node *n = parse_expr(p);
            if (peek(p)->kind == TOK_RPAREN) advance(p);
            return n;
        }
        case TOK_MINUS: {
            advance(p);
            Node *n = nd_new(ND_UMINUS);
            n->left = parse_primary(p);
            return n;
        }
        case TOK_NUM:   advance(p); { Node *n = nd_new(ND_NUM); n->num = t->num; return n; }
        case TOK_STR:   advance(p); { Node *n = nd_new(ND_STR); n->str = arena_strdup(p->arena, t->text); return n; }
        case TOK_DICT:  advance(p); return nd_new(ND_DICT);
        case TOK_KEYS:  advance(p); { Node *n = nd_new(ND_KEYS);  n->left = parse_postfix(p); return n; }
        case TOK_NOT:   advance(p); { Node *n = nd_new(ND_NOT);   n->left = parse_primary(p); return n; }
        case TOK_IDENT: 
            advance(p); { Node *n = nd_new(ND_VAR); n->name = arena_intern(p->arena, t->text); return n; }
        case TOK_ASK:   advance(p); return nd_new(ND_ASK);
        case TOK_ARG:   advance(p); return nd_new(ND_ARG);
        case TOK_LEN:   advance(p); { Node *n = nd_new(ND_LEN);   n->left = parse_postfix(p); return n; }
        case TOK_ORD:   advance(p); { Node *n = nd_new(ND_ORD);   n->left = parse_postfix(p); return n; }
        case TOK_CHR:   advance(p); { Node *n = nd_new(ND_CHR);   n->left = parse_postfix(p); return n; }
        case TOK_TONUM: advance(p); { Node *n = nd_new(ND_TONUM); n->left = parse_postfix(p); return n; }
        case TOK_TOSTR: advance(p); { Node *n = nd_new(ND_TOSTR); n->left = parse_postfix(p); return n; }
        case TOK_SPLIT: advance(p); { Node *n = nd_new(ND_SPLIT); n->left = parse_primary(p); n->right = parse_primary(p); return n; }
        case TOK_TRIM:  advance(p); { Node *n = nd_new(ND_TRIM);  n->left = parse_primary(p); return n; }
        case TOK_REPLACE: {
            advance(p);
            Node *n = nd_new(ND_REPLACE);
            n->left = parse_primary(p);
            n->body_cap = 2;
            n->body = arena_alloc(p->arena, 2 * sizeof(Node*));
            nd_push(n, parse_primary(p));
            nd_push(n, parse_primary(p));
            return n;
        }
        case TOK_SAFE:  advance(p); { Node *n = nd_new(ND_SAFE);  n->left = parse_primary(p); return n; }
        case TOK_SLICE: {
            advance(p);
            Node *n = nd_new(ND_SLICE);
            n->left = parse_primary(p);
            n->body_cap = 2;
            n->body = arena_alloc(p->arena, 2 * sizeof(Node*));
            nd_push(n, parse_primary(p));
            nd_push(n, parse_primary(p));
            return n;
        }
        case TOK_TYPE:  advance(p); { Node *n = nd_new(ND_TYPE);  n->left = parse_primary(p); return n; }
        case TOK_TIME:  advance(p); return nd_new(ND_TIME);
        case TOK_SYS:   advance(p); { Node *n = nd_new(ND_SYS);   n->left = parse_postfix(p); return n; }
        case TOK_REF:   advance(p); { Node *n = nd_new(ND_REF);   n->left = parse_primary(p); return n; }
        case TOK_ENV:   advance(p); { Node *n = nd_new(ND_ENV);   n->left = parse_postfix(p); return n; }
        case TOK_EXIT:  advance(p); { Node *n = nd_new(ND_EXIT);  n->left = parse_postfix(p); return n; }
        case TOK_LIST:  advance(p); return nd_new(ND_LIST);
        case TOK_DO: {
            Token *do_tok = t;
            advance(p);
            Node *fn = nd_new(ND_DO);

            /* check for do->attribute */
            if (peek(p)->kind == TOK_ARROW) {
                advance(p);
                if (peek(p)->kind == TOK_IDENT || peek(p)->kind == TOK_REF || peek(p)->kind == TOK_PTR || peek(p)->kind == TOK_BIT)
                    fn->mod_tag = arena_intern(p->arena, advance(p)->text);
            }

            fn->name = arena_intern(p->arena, advance(p)->text);

            /* check for return type: do func->type */
            if (peek(p)->kind == TOK_ARROW) {
                advance(p);
                if (peek(p)->kind == TOK_IDENT || peek(p)->kind == TOK_REF || peek(p)->kind == TOK_PTR || peek(p)->kind == TOK_BIT)
                    fn->type_tag = arena_intern(p->arena, advance(p)->text);
            }

            fn->body_cap = 8;
            fn->body = arena_alloc(p->arena, fn->body_cap * sizeof(Node*));
            while (peek(p)->kind == TOK_IDENT) {
                Node *param = nd_new(ND_VAR);
                param->name = arena_intern(p->arena, advance(p)->text);
                if (is_all_caps(param->name)) {
                    /* Parameters in ALL_CAPS are immutable within the function scope */
                    track_constant(p, param->name, p->tokens[p->pos-1].pos, p->tokens[p->pos-1].line);
                }
                if (peek(p)->kind == TOK_ARROW) {
                    advance(p);
                    if (peek(p)->kind == TOK_IDENT || peek(p)->kind == TOK_REF || peek(p)->kind == TOK_PTR || peek(p)->kind == TOK_BIT)
                        param->type_tag = arena_intern(p->arena, advance(p)->text);
                }
                nd_push(fn, param);
            }
            skip_newlines(p);
            expect(p, TOK_INDENT);
            p->in_func++;
            Node *old_func = p->current_func;
            p->current_func = fn;
            fn->left = parse_block(p, true);
            p->current_func = old_func; p->in_func--;
            expect(p, TOK_DEDENT);

            /* Compile-time validation: Ensure return type matches the last expression */
            if (fn->type_tag) {
                Node *block = fn->left;
                if (block->body_count > 0) {
                    Node *last = block->body[block->body_count - 1];
                    const char *actual = infer_node_type(last, fn);
                    bool compatible = (strcmp(actual, fn->type_tag) == 0) || 
                                      (strcmp(fn->type_tag, "bit") == 0 && strcmp(actual, "num") == 0);
                    if (strcmp(actual, "unknown") != 0 && !compatible) {
                        report_error(p->filename, p->src, do_tok->pos, do_tok->line, 
                            "return type mismatch in function '%s': expected '%s', found '%s'", 
                            fn->name, fn->type_tag, actual);
                    }
                }
            }
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
            at->right = parse_primary(p);
            expr = at;
        } else if (peek(p)->kind == TOK_SET) {
            advance(p);
            Node *set = nd_new(ND_SET);
            set->left = expr;
            set->body_cap = 4;
            set->body = arena_alloc(p->arena, set->body_cap * sizeof(Node*));
            nd_push(set, parse_primary(p));
            nd_push(set, parse_primary(p));
            expr = set;
        } else if (peek(p)->kind == TOK_KEY) {
            advance(p);
            Node *key = nd_new(ND_KEY);
            key->left = expr;
            key->body_cap = 2;
            key->body = arena_alloc(p->arena, 2 * sizeof(Node*));
            nd_push(key, parse_primary(p));
            nd_push(key, parse_primary(p));
            expr = key;
        } else if (peek(p)->kind == TOK_PUT) {
            advance(p);
            Node *put = nd_new(ND_PUT);
            put->left = expr;
            put->right = parse_expr(p);
            if (peek(p)->kind == TOK_APPEND) {
                advance(p);
                put->op = TOK_APPEND;
            }
            expr = put;
        } else if (peek(p)->kind == TOK_ASK && expr->kind != ND_ASK) {
            advance(p);
            Node *af = nd_new(ND_ASKFILE);
            af->left = expr;
            expr = af;
        } else if (peek(p)->kind != TOK_NEWLINE && peek(p)->kind != TOK_SEMICOLON && peek(p)->kind != TOK_DEDENT && peek(p)->kind != TOK_EOF &&
                   (peek(p)->kind == TOK_IDENT || peek(p)->kind == TOK_NUM || peek(p)->kind == TOK_LPAREN ||
                   peek(p)->kind == TOK_STR || peek(p)->kind == TOK_ASK ||
                   peek(p)->kind == TOK_LIST || peek(p)->kind == TOK_ARG ||
                   peek(p)->kind == TOK_LEN || peek(p)->kind == TOK_ORD ||
                   peek(p)->kind == TOK_CHR || peek(p)->kind == TOK_TONUM ||
                   peek(p)->kind == TOK_TOSTR || peek(p)->kind == TOK_SLICE ||
                   peek(p)->kind == TOK_SPLIT || peek(p)->kind == TOK_TRIM || peek(p)->kind == TOK_REPLACE ||
                   peek(p)->kind == TOK_SAFE || peek(p)->kind == TOK_REF ||
                   peek(p)->kind == TOK_TYPE || peek(p)->kind == TOK_TIME ||
                   peek(p)->kind == TOK_DICT || peek(p)->kind == TOK_SYS ||
                   peek(p)->kind == TOK_ENV || peek(p)->kind == TOK_EXIT)) {
            if (expr->kind != ND_VAR) break;

            if (peek(p)->kind == TOK_IDENT && (p->tokens[p->pos+1].kind == TOK_EQ || p->tokens[p->pos+1].kind == TOK_ARROW)) {
                break;
            }

            Node *call = nd_new(ND_CALL);
            call->left = expr;
            call->body_cap = 8;
            call->body = arena_alloc(p->arena, call->body_cap * sizeof(Node*));
            while (peek(p)->kind != TOK_NEWLINE && peek(p)->kind != TOK_SEMICOLON && peek(p)->kind != TOK_DEDENT && peek(p)->kind != TOK_EOF &&
                   (peek(p)->kind == TOK_IDENT || peek(p)->kind == TOK_NUM || peek(p)->kind == TOK_LPAREN ||
                   peek(p)->kind == TOK_STR || peek(p)->kind == TOK_ASK ||
                   peek(p)->kind == TOK_LIST || peek(p)->kind == TOK_ARG ||
                   peek(p)->kind == TOK_LEN || peek(p)->kind == TOK_ORD ||
                   peek(p)->kind == TOK_CHR || peek(p)->kind == TOK_TONUM ||
                   peek(p)->kind == TOK_TOSTR || peek(p)->kind == TOK_SLICE ||
                   peek(p)->kind == TOK_SPLIT || peek(p)->kind == TOK_TRIM || peek(p)->kind == TOK_REPLACE ||
                   peek(p)->kind == TOK_SAFE || peek(p)->kind == TOK_REF ||
                   peek(p)->kind == TOK_TYPE || peek(p)->kind == TOK_TIME ||
                   peek(p)->kind == TOK_DICT || peek(p)->kind == TOK_SYS ||
                   peek(p)->kind == TOK_ENV || peek(p)->kind == TOK_EXIT)) {
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
           peek(p)->kind == TOK_GT    || peek(p)->kind == TOK_EQEQ  ||
           peek(p)->kind == TOK_LTE   || peek(p)->kind == TOK_GTE   ||
           peek(p)->kind == TOK_NE    || peek(p)->kind == TOK_AND   ||
           peek(p)->kind == TOK_OR) {
        TokKind op = advance(p)->kind;
        Node *right = parse_postfix(p);
        Node *bin = nd_new(ND_BINOP);
        bin->op = op;
        bin->left = left;
        bin->right = right;
        left = bin;
    }

    /* assignment: var = expr OR var type = expr */
    bool is_typed = (left->kind == ND_VAR && (peek(p)->kind == TOK_IDENT || peek(p)->kind == TOK_REF || peek(p)->kind == TOK_PTR || peek(p)->kind == TOK_BIT) &&
                    (p->tokens[p->pos+1].kind == TOK_EQ || p->tokens[p->pos+1].kind == TOK_ARROW));
    bool is_normal = (peek(p)->kind == TOK_EQ && (left->kind == ND_VAR || left->kind == ND_AT));

    if (is_typed || is_normal) {
        Node *assign = nd_new(ND_ASSIGN);
        assign->left = left;
        Token *op = peek(p);

        if (is_typed) {
            if (left->kind == ND_VAR && is_all_caps(left->name))
                track_constant(p, left->name, op->pos, op->line);
            left->type_tag = arena_intern(p->arena, advance(p)->text);
            advance(p);
        } else {
            if (left->kind == ND_VAR && is_all_caps(left->name))
                track_constant(p, left->name, op->pos, op->line);
            advance(p);
        }

        assign->right = parse_expr(p);

        /* Implementation of Assignment Type Checking */
        if (left->type_tag) {
            const char *actual = infer_node_type(assign->right, p->current_func);
            bool compatible = (strcmp(actual, left->type_tag) == 0) || 
                              (strcmp(left->type_tag, "bit") == 0 && strcmp(actual, "num") == 0);
            if (strcmp(actual, "unknown") != 0 && !compatible) {
                report_error(p->filename, p->src, op->pos, op->line, 
                    "type mismatch in assignment to '%s': expected '%s', found '%s'", 
                    left->name, left->type_tag, actual);
            }
        }
        return assign;
    }

    return left;
}

/* ──────────────────────── Compile-time IF macros ──────────────────────── */

static void skip_block_body(Parser *p) {
    int depth = 0;
    while (peek(p)->kind != TOK_EOF) {
        if (peek(p)->kind == TOK_INDENT) {
            depth++;
            advance(p);
        } else if (peek(p)->kind == TOK_DEDENT) {
            if (depth == 0) break;
            depth--;
            advance(p);
        } else {
            advance(p);
        }
    }
}

static bool resolve_condition(const char *name) {
    if (strcmp(name, "WIN") == 0) {
#ifdef _WIN32
        return true;
#else
        return false;
#endif
    }
    if (strcmp(name, "UNIX") == 0) {
#ifdef _WIN32
        return false;
#else
        return true;
#endif
    }
    for (int i = 0; i < cli_define_count; i++) {
        if (strcmp(cli_defines[i], name) == 0) return true;
    }
    return false;
}

static Node *parse_when_stmt(Parser *p) {
    Node *result = nd_new(ND_BLOCK);
    bool found = false;

    advance(p); /* consume "IF" */

    while (true) {
        /* read condition identifier */
        Token *cond = peek(p);
        if (cond->kind != TOK_IDENT || !is_all_caps(cond->text)) {
            report_error(p->filename, p->src, cond->pos, cond->line,
                "IF requires an ALL_CAPS condition, got '%s'", cond->text ? cond->text : "");
            exit(1);
        }
        advance(p);

        bool taken = resolve_condition(cond->text) && !found;
        skip_newlines(p);

        if (taken) {
            found = true;
            expect(p, TOK_INDENT);
            Node *body = parse_block(p, p->in_func > 0);
            expect(p, TOK_DEDENT);
            for (int i = 0; i < body->body_count; i++)
                nd_push(result, body->body[i]);
        } else {
            expect(p, TOK_INDENT);
            skip_block_body(p);
            expect(p, TOK_DEDENT);
        }

        /* check for ELSE / ELSE IF chain */
        skip_newlines(p);
        if (peek(p)->kind != TOK_IDENT || strcmp(peek(p)->text, "ELSE") != 0)
            break;

        advance(p); /* consume "ELSE" */
        skip_newlines(p);

        if (peek(p)->kind == TOK_IDENT && strcmp(peek(p)->text, "IF") == 0) {
            advance(p); /* consume "IF", loop to read condition */
            continue;
        }

        /* plain ELSE — final fallback */
        if (!found) {
            expect(p, TOK_INDENT);
            Node *body = parse_block(p, p->in_func > 0);
            expect(p, TOK_DEDENT);
            for (int i = 0; i < body->body_count; i++)
                nd_push(result, body->body[i]);
        } else {
            expect(p, TOK_INDENT);
            skip_block_body(p);
            expect(p, TOK_DEDENT);
        }
        break;
    }

    return result;
}

static Node *parse_stmt(Parser *p) {
    skip_newlines(p);
    Token *t = peek(p);

    /* compile-time IF macro */
    if (t->kind == TOK_IDENT && strcmp(t->text, "IF") == 0) {
        return parse_when_stmt(p);
    }

    if (t->kind == TOK_FOR) {
        advance(p);
        Node *n = nd_new(ND_FOR);
        n->name = arena_strdup(p->arena, advance(p)->text); // loop variable
        expect(p, TOK_IN);
        n->left = parse_expr(p); // the list/dict
        skip_newlines(p);
        expect(p, TOK_INDENT);
        nd_push(n, parse_block(p, false));
        expect(p, TOK_DEDENT);
        return n;
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

    if (t->kind == TOK_BREAK || (t->kind == TOK_IDENT && strcmp(t->text, "break") == 0)) {
        advance(p);
        return nd_new(ND_BREAK);
    }

    if (t->kind == TOK_SKIP) {
        advance(p);
        return nd_new(ND_SKIP);
    }

    if (t->kind == TOK_OUT) {
        advance(p);
        Node *n = nd_new(ND_OUT);
        n->left = parse_expr(p);
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

    /* expression statement (with implicit print outside function bodies) */
    Node *expr = parse_expr(p);
    if (p->in_func == 0 && expr->kind != ND_ASSIGN && expr->kind != ND_DO && 
        expr->kind != ND_SET && expr->kind != ND_KEY && expr->kind != ND_PUT && 
        expr->kind != ND_EXIT && expr->kind != ND_BLOCK && expr->kind != ND_BREAK &&
        expr->kind != ND_OUT && expr->kind != ND_SKIP && expr->kind != ND_IF &&
        expr->kind != ND_WHILE && expr->kind != ND_FOR) {
        Node *print = nd_new(ND_PRINT);
        print->left = expr;
        return print;
    }
    return expr;
}

static Node *parse_block(Parser *p, bool is_function) {
    Node *block = nd_new(ND_BLOCK);
    while (peek(p)->kind != TOK_DEDENT && peek(p)->kind != TOK_EOF) {
        if (peek(p)->kind == TOK_NEWLINE || peek(p)->kind == TOK_SEMICOLON) { advance(p); continue; }

        Node *stmt = parse_stmt(p);
        skip_newlines(p);

        nd_push(block, stmt);
        skip_newlines(p);
    }

    /* Implicit return logic: the last expression in a function shouldn't print, it's the return value */
    if (is_function && block->body_count > 0) {
        block->body[block->body_count - 1] = strip_prints(block->body[block->body_count - 1]);
    }
    return block;
}

Node *parse_program(Parser *p) {
    extern int trace;
    parser_current_filename = p->filename;
    Node *prog = nd_new(ND_BLOCK);

    if (trace) fprintf(stderr, "[PARSER] First token kind=%d, text='%s'\n", peek(p)->kind, peek(p)->text ? peek(p)->text : "(null)");
    if (peek(p)->kind == TOK_SHEBANG) {
        if (trace) fprintf(stderr, "[PARSER] Skipping shebang token\n");
        advance(p);
        skip_newlines(p);
    }

    while (peek(p)->kind != TOK_EOF) {
        if (peek(p)->kind == TOK_NEWLINE || peek(p)->kind == TOK_SEMICOLON) { advance(p); continue; }
        if (peek(p)->kind == TOK_SHEBANG) { advance(p); continue; }  /* Skip shebangs from pasted files */
        nd_push(prog, parse_stmt(p));
        skip_newlines(p);
    }
    return prog;
}
