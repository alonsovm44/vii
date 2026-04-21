#include "vii.h"

/* global CLI defines for IF macros */
char **cli_defines = NULL;
int    cli_define_count = 0;
static int cli_define_cap = 0;

static void add_define(const char *name) {
    if (cli_define_count >= cli_define_cap) {
        cli_define_cap = cli_define_cap ? cli_define_cap * 2 : 8;
        cli_defines = realloc(cli_defines, cli_define_cap * sizeof(char*));
    }
    cli_defines[cli_define_count++] = strdup(name);
}

int main(int argc, char **argv) {
    enable_ansi_colors();
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
        } else if (argv[i][0] != '-') {
            if (!input_path) input_path = argv[i];
        }
    }

    if (!input_path && strcmp(argv[1], "--version") != 0 && strcmp(argv[1], "--help") != 0) goto usage;

    if (strcmp(argv[1], "--version") == 0) {
        printf("vii 1.2.5\n");
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
    eval(prog, global);

    free(src);
    return 0;

usage:
    fprintf(stderr, "Usage: vii <file.vii> [-o program] [args...]\n");
    return 1;
}
