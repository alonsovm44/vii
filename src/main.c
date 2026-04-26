#include "vii.h"

/* global CLI defines for IF macros */
char **cli_defines = NULL;
int    cli_define_count = 0;
static int cli_define_cap = 0;

int trace = 0; // Global definition of trace
FILE *log_fp = NULL; // Log file for trace output

Arena *global_arena = NULL;

typedef struct InternEntry {
    char *str;
    struct InternEntry *next;
} InternEntry;

static InternEntry *intern_table[1024];

char* arena_intern(Arena *a, const char *s) {
    unsigned h = 0;
    for (int i = 0; s[i]; i++) h = h * 31 + (unsigned char)s[i];
    h %= 1024;
    for (InternEntry *e = intern_table[h]; e; e = e->next) {
        if (strcmp(e->str, s) == 0) return e->str;
    }
    InternEntry *new_entry = arena_alloc(a, sizeof(InternEntry));
    new_entry->str = arena_strdup(a, s);
    new_entry->next = intern_table[h];
    intern_table[h] = new_entry;
    return new_entry->str;
}

Arena* arena_create(size_t size) {
    Arena *a = malloc(sizeof(Arena));
    a->capacity = size;
    a->offset = 0;
    a->data = malloc(size);
    return a;
}

void* arena_alloc(Arena *a, size_t size) {
    size_t aligned = (size + 7) & ~7;
    if (a->offset + aligned > a->capacity) {
        fprintf(stderr, "\n--- FATAL: Arena Memory Exhausted ---\n");
        fprintf(stderr, "Capacity: %zu bytes\n", a->capacity);
        fprintf(stderr, "Current Offset: %zu bytes\n", a->offset);
        fprintf(stderr, "Requested: %zu bytes\n", size);
        fprintf(stderr, "Hint: This usually indicates an infinite loop creating\n");
        fprintf(stderr, "dictionaries or strings in your Vii code.\n");
        fprintf(stderr, "Check for circular 'paste' calls or lexer loops.\n");
        fprintf(stderr, "---------------------------------------\n");
        exit(1);
    }
    void *ptr = a->data + a->offset;
    a->offset += aligned;
    memset(ptr, 0, aligned);
    return ptr;
}

char* arena_strdup(Arena *a, const char *s) {
    size_t len = strlen(s) + 1;
    char *dest = arena_alloc(a, len);
    memcpy(dest, s, len);
    return dest;
}

static void add_define(const char *name) {
    if (cli_define_count >= cli_define_cap) {
        int old_cap = cli_define_cap;
        cli_define_cap = cli_define_cap ? cli_define_cap * 2 : 8;
        char **new_defines = arena_alloc(global_arena, cli_define_cap * sizeof(char*));
        if (cli_defines) memcpy(new_defines, cli_defines, old_cap * sizeof(char*));
        cli_defines = new_defines;
    }
    cli_defines[cli_define_count++] = arena_strdup(global_arena, name);
}

int main(int argc, char **argv) {
    enable_ansi_colors();
    global_arena = arena_create(512 * 1024 * 1024); // 512MB Arena

    const char *input_path = NULL;
    const char *output_name = NULL;
    bool debug_ast = false;
    bool keep_c = false;

    if (argc < 2) goto usage;

    /* Argument Parsing */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            debug_ast = true;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) output_name = argv[++i];
            else { fprintf(stderr, "Error: -o requires a filename\n"); return 1; }
        } else if (strcmp(argv[i], "-k") == 0 || strcmp(argv[i], "--keep") == 0) {
            keep_c = true;
        } else if (strcmp(argv[i], "-D") == 0 || strcmp(argv[i], "--define") == 0) {
            if (i + 1 < argc) add_define(argv[++i]);
            else { fprintf(stderr, "Error: -D requires a name\n"); return 1; }
        } else if (strcmp(argv[i], "--trace") == 0) {
            trace = true;
        } else if (strcmp(argv[i], "--log") == 0) {
            if (i + 1 < argc) {
                log_fp = fopen(argv[++i], "w");
                if (!log_fp) { fprintf(stderr, "Error: cannot open log file %s\n", argv[i]); return 1; }
            } else { fprintf(stderr, "Error: --log requires a filename\n"); return 1; }
        } else if (argv[i][0] != '-') {
            if (!input_path) input_path = argv[i];
        }
    }

    if (!input_path && strcmp(argv[1], "--version") != 0 && strcmp(argv[1], "--help") != 0) goto usage;

    if (strcmp(argv[1], "--version") == 0) {
        printf("vii 1.3.0\n");
        return 0;
    }
    if (strcmp(argv[1], "--help") == 0) {
        printf("vii - a minimalist programming language\n");
        printf("Usage: vii <file.vii> [-o program] [-k] [args...]\n");
        printf("       vii --version\n");
        printf("       vii --help\n");
        printf("       vii --debug <file.vii>\n\n");
        printf("Options:\n");
        printf("  -o <name>      Compile to executable\n");
        printf("  -k, --keep     Keep transpiled .c source\n");
        printf("  -D <name>      Define compile-time flag for IF macros\n\n");
        
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
        if (strcmp(argv[i], "--debug") == 0) continue;
        if (strcmp(argv[i], "-o") == 0) { i++; continue; }
        if (strcmp(argv[i], "-k") == 0 || strcmp(argv[i], "--keep") == 0) continue;
        if (strcmp(argv[i], "-D") == 0 || strcmp(argv[i], "--define") == 0) { i++; continue; }
        if (strcmp(argv[i], "--trace") == 0) continue;
        if (input_path && strcmp(argv[i], input_path) == 0) continue;

        if (cli_args->item_count >= cli_args->item_cap) {
            val_list_grow(cli_args);
        }
        cli_args->items[cli_args->item_count++] = val_str(argv[i]);
    }

    char *src = read_file(input_path);
    if (trace) fprintf(stderr, "[TRACE] Read %zu bytes from %s\n", strlen(src), input_path);
    Lexer lexer = { .src = src, .pos = 0, .filename = input_path, .arena = global_arena };
    if (trace) fprintf(stderr, "[TRACE] Starting lexer...\n");
    lex(&lexer, input_path);
    if (trace) { fprintf(stderr, "[TRACE] Lexed %d tokens\n", lexer.tok_count); if (log_fp) fprintf(log_fp, "[TRACE] Lexed %d tokens\n", lexer.tok_count); }

    Parser parser = { .tokens = lexer.tokens, .pos = 0, .src = src, .filename = input_path, .arena = global_arena };
    if (trace) { fprintf(stderr, "[TRACE] Starting parser...\n"); if (log_fp) fprintf(log_fp, "[TRACE] Starting parser...\n"); }
    Node *prog = parse_program(&parser);
    if (trace) { fprintf(stderr, "[TRACE] Parsed AST with kind=%d\n", prog->kind); if (log_fp) fprintf(log_fp, "[TRACE] Parsed AST with kind=%d\n", prog->kind); }

    if (debug_ast) {
        FILE *df = fopen("debug_ast.json", "w");
        if (df) {
            dump_ast_json(prog, df, 0);
            fclose(df);
            printf("AST dumped to debug_ast.json\n");
        }
    }

    if (output_name) {
        compile_to_bin(prog, output_name, keep_c);
        free(src);
        return 0;
    }

    /* Otherwise, Interpret */
    Table *global = table_new(NULL);
    /* Bind CLI args as 'arg' for bootstrapping compiler compatibility */
    extern Value *cli_args;
    if (trace) { fprintf(stderr, "[TRACE] cli_args kind=%d (VAL_LIST=%d)\n", cli_args ? cli_args->kind : -1, VAL_LIST); if (log_fp) fprintf(log_fp, "[TRACE] cli_args kind=%d (VAL_LIST=%d)\n", cli_args ? cli_args->kind : -1, VAL_LIST); }
    table_set(global, "arg", cli_args);
    if (trace) { fprintf(stderr, "[TRACE] Starting interpreter...\n"); if (log_fp) fprintf(log_fp, "[TRACE] Starting interpreter...\n"); }
    Value *res = eval(prog, global); // Evaluate the entire program
    if (trace) { fprintf(stderr, "[TRACE] Interpreter returned kind=%d\n", res ? res->kind : -1); if (log_fp) fprintf(log_fp, "[TRACE] Interpreter returned kind=%d\n", res ? res->kind : -1); }

    // Check if the last statement in the program was an implicit print.
    // If so, it has already been printed by the ND_PRINT node.
    bool last_stmt_was_implicit_print = false;
    if (prog->kind == ND_BLOCK && prog->body_count > 0) {
        if (prog->body[prog->body_count - 1]->kind == ND_PRINT) {
            last_stmt_was_implicit_print = true;
        }
    }

    if (!last_stmt_was_implicit_print && res && res->kind != VAL_NONE && res->kind != VAL_BREAK) {
        val_print(res); // Only print if not already handled by an implicit print
        printf("\n"); 
    }

    free(src);
    if (log_fp) fclose(log_fp);
    return 0;

usage:
    fprintf(stderr, "Usage: vii <file.vii> [-o program] [args...]\n");
    return 1;
}
