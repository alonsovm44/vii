#include "io.h"

void enable_ansi_colors(void) {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) {
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
}

void report_error(const char *filename, const char *src, int pos, int line, const char *fmt, ...) {
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
    for (int i = start; i < pos; i++) { if (src[i] == '\r') caret--; }
    for (int i = 0; i < caret; i++) fprintf(stderr, " ");
    fprintf(stderr, COL_GRN "^" COL_RST "\n");
    exit(1);
}

char *read_file(const char *path) {
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
