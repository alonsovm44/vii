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
        fprintf(stderr, "Capacity: %lu bytes\n", (unsigned long)a->capacity);
        fprintf(stderr, "Current Offset: %lu bytes\n", (unsigned long)a->offset);
        fprintf(stderr, "Requested: %lu bytes\n", (unsigned long)size);
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

/* Check if this binary contains a bundled script and extract it */
static char* extract_bundled_script(const char *exe_path, size_t *out_len) {
    FILE *f = fopen(exe_path, "rb");
    if (!f) return NULL;
    
    /* Seek to end to read footer */
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    
    /* Footer is 16 bytes: script_offset (8) + magic (8) */
    if (file_size < 16) {
        fclose(f);
        return NULL;
    }
    
    /* Read footer */
    fseek(f, -16, SEEK_END);
    uint64_t script_offset, magic;
    fread(&script_offset, sizeof(script_offset), 1, f);
    fread(&magic, sizeof(magic), 1, f);
    fclose(f);
    
    /* Check magic number: "VIIBUNDL" */
    if (magic != 0x56494942554E444C) {
        return NULL;
    }
    
    /* Validate script offset */
    if (script_offset >= file_size - 16) {
        return NULL;
    }
    
    /* Read embedded script */
    f = fopen(exe_path, "rb");
    if (!f) return NULL;
    
    size_t script_len = file_size - 16 - script_offset;
    char *script = malloc(script_len + 1);
    if (!script) {
        fclose(f);
        return NULL;
    }
    
    fseek(f, script_offset, SEEK_SET);
    fread(script, 1, script_len, f);
    script[script_len] = '\0';
    fclose(f);
    
    *out_len = script_len;
    return script;
}

int main(int argc, char **argv) {
    enable_ansi_colors();
    global_arena = arena_create(512 * 1024 * 1024); // 512MB Arena

    /* Check if running as a bundled binary */
    char exe_path[1024];
#ifdef _WIN32
    GetModuleFileName(NULL, exe_path, sizeof(exe_path));
#else
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len != -1) exe_path[len] = '\0';
    else strcpy(exe_path, argv[0]);
#endif
    
    size_t bundled_len = 0;
    char *bundled_script = extract_bundled_script(exe_path, &bundled_len);
    if (bundled_script) {
        /* Running as bundled - execute embedded script */
        if (trace) fprintf(stderr, "[TRACE] Running bundled script (%lu bytes)\n", (unsigned long)bundled_len);
        
        Lexer lexer = { .src = bundled_script, .pos = 0, .filename = "<bundled>", .arena = global_arena };
        lex(&lexer, "<bundled>");
        
        Parser parser = { .tokens = lexer.tokens, .pos = 0, .src = bundled_script, .filename = "<bundled>", .arena = global_arena };
        Node *prog = parse_program(&parser);
        
        Table *global = table_new(NULL);
        cli_args = val_list();
        for (int i = 1; i < argc; i++) {
            if (cli_args->item_count >= cli_args->item_cap) val_list_grow(cli_args);
            cli_args->items[cli_args->item_count++] = val_str(argv[i]);
        }
        table_set(global, "arg", cli_args);
        
        Value *res = eval(prog, global);
        if (res && res->kind != VAL_NONE && res->kind != VAL_BREAK) {
            val_print(res);
            printf("\n");
        }
        
        free(bundled_script);
        return 0;
    }

    const char *input_path = NULL;
    const char *bundle_name = NULL;
    bool debug_ast = false;

    if (argc < 2) goto usage;

    /* Argument Parsing */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            debug_ast = true;
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
        } else if (strcmp(argv[i], "--bundle") == 0) {
            if (i + 1 < argc) bundle_name = argv[++i];
            else { fprintf(stderr, "Error: --bundle requires a filename\n"); return 1; }
        } else if (argv[i][0] != '-') {
            if (!input_path) input_path = argv[i];
        }
    }

    if (!input_path && strcmp(argv[1], "--version") != 0 && strcmp(argv[1], "--help") != 0) goto usage;

    if (strcmp(argv[1], "--version") == 0) {
        printf("vii 1.4.1\n");
        return 0;
    }
    if (strcmp(argv[1], "--help") == 0) {
        printf("vii - a minimalist programming language\n");
        printf("Usage: vii <file.vii> [args...]\n");
        printf("       vii --version\n");
        printf("       vii --help\n");
        printf("       vii --debug <file.vii>\n");
        printf("       vii <file.vii> --bundle <output.exe>\n\n");
        printf("Options:\n");
        printf("  --bundle <out> Bundle script + interpreter into standalone binary\n");
        printf("  -D <name>      Define compile-time flag for IF macros\n\n");
        
        return 0;
    }

    /* Windows .exe handling for bundle */
#ifdef _WIN32
    char win_bundle[512];
    if (bundle_name && !strstr(bundle_name, ".exe") && !strstr(bundle_name, ".EXE")) {
        snprintf(win_bundle, sizeof(win_bundle), "%s.exe", bundle_name);
        bundle_name = win_bundle;
    }
#endif

    /* Handle bundling: vii script.vii --bundle output.exe */
    if (bundle_name) {
        if (!input_path) {
            fprintf(stderr, "Error: --bundle requires an input file\n");
            return 1;
        }
        
        /* Read the script content */
        char *script_src = read_file(input_path);
        if (!script_src) {
            fprintf(stderr, "Error: cannot read input file %s\n", input_path);
            return 1;
        }
        size_t script_len = strlen(script_src);
        
        /* Get path to current executable */
        char exe_path[1024];
#ifdef _WIN32
        GetModuleFileName(NULL, exe_path, sizeof(exe_path));
#else
        ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (len != -1) exe_path[len] = '\0';
        else strcpy(exe_path, argv[0]);
#endif
        
        /* Read the interpreter binary */
        FILE *fexe = fopen(exe_path, "rb");
        if (!fexe) {
            fprintf(stderr, "Error: cannot read interpreter binary\n");
            free(script_src);
            return 1;
        }
        
        fseek(fexe, 0, SEEK_END);
        size_t exe_len = ftell(fexe);
        fseek(fexe, 0, SEEK_SET);
        
        char *exe_data = malloc(exe_len);
        if (!exe_data) {
            fprintf(stderr, "Error: out of memory\n");
            fclose(fexe);
            free(script_src);
            return 1;
        }
        fread(exe_data, 1, exe_len, fexe);
        fclose(fexe);
        
        /* Create the bundle: exe + script + footer */
        FILE *fout = fopen(bundle_name, "wb");
        if (!fout) {
            fprintf(stderr, "Error: cannot create bundle file %s\n", bundle_name);
            free(exe_data);
            free(script_src);
            return 1;
        }
        
        /* Write interpreter */
        fwrite(exe_data, 1, exe_len, fout);
        
        /* Write script */
        fwrite(script_src, 1, script_len, fout);
        
        /* Write bundle footer: magic + script offset */
        uint64_t script_offset = exe_len;
        uint64_t magic = 0x56494942554E444C; /* "VIIBUNDL" */
        fwrite(&script_offset, sizeof(script_offset), 1, fout);
        fwrite(&magic, sizeof(magic), 1, fout);
        
        fclose(fout);
        free(exe_data);
        free(script_src);
        
        /* Make executable on Unix */
#ifndef _WIN32
        chmod(bundle_name, 0755);
#endif
        
        printf("Bundle created: %s\n", bundle_name);
        return 0;
    }

    /* build CLI arg list */
    cli_args = val_list();
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) continue;
        if (strcmp(argv[i], "--bundle") == 0) { i++; continue; }
        if (strcmp(argv[i], "-D") == 0 || strcmp(argv[i], "--define") == 0) { i++; continue; }
        if (strcmp(argv[i], "--trace") == 0) continue;
        if (input_path && strcmp(argv[i], input_path) == 0) continue;

        if (cli_args->item_count >= cli_args->item_cap) {
            val_list_grow(cli_args);
        }
        cli_args->items[cli_args->item_count++] = val_str(argv[i]);
    }

    char *src = read_file(input_path);
    if (trace) fprintf(stderr, "[TRACE] Read %lu bytes from %s\n", (unsigned long)strlen(src), input_path);
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

    /* Interpret the program */
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
    fprintf(stderr, "Usage: vii <file.vii> [args...]\n");
    fprintf(stderr, "       vii <file.vii> --bundle <output.exe>\n");
    return 1;
}
