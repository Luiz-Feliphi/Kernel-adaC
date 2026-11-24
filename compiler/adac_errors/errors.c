/*
 * errors.c - Diagnostic printing.
 */
#include "adac_errors/errors.h"
#ifndef _WIN32
#include <unistd.h>
#endif

/* ---------------- diagnostics ---------------- */
static int  g_errors = 0, g_warnings = 0;
static bool g_color = false;

void diag_set_color(bool on) { g_color = on; }
int  diag_error_count(void) { return g_errors; }
int  diag_warning_count(void) { return g_warnings; }

#define C_RED   (g_color ? "\x1b[1;31m" : "")
#define C_YEL   (g_color ? "\x1b[1;33m" : "")
#define C_CYAN  (g_color ? "\x1b[1;36m" : "")
#define C_BOLD  (g_color ? "\x1b[1m"    : "")
#define C_RST   (g_color ? "\x1b[0m"    : "")

/* prints the source line with a ^ under the problem column */
static void print_snippet(Loc loc) {
    if (!loc.src || loc.line <= 0) return;
    const char *p = loc.src->text;
    int line = 1;
    while (*p && line < loc.line) {
        if (*p == '\n') line++;
        p++;
    }
    const char *e = p;
    while (*e && *e != '\n' && *e != '\r') e++;
    fprintf(stderr, " %5d | %.*s\n", loc.line, (int)(e - p), p);
    fprintf(stderr, " %5s | ", "");
    for (int i = 1; i < loc.col && p + i - 1 < e; i++) fputc(p[i - 1] == '\t' ? '\t' : ' ', stderr);
    fprintf(stderr, "%s^%s\n", C_RED, C_RST);
}

static void vreport(const char *kind, const char *color, Loc loc, const char *fmt, va_list ap, bool snippet) {
    if (loc.src) fprintf(stderr, "%s%s:%d:%d:%s ", C_BOLD, loc.src->name, loc.line, loc.col, C_RST);
    fprintf(stderr, "%s%s:%s ", color, kind, C_RST);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    if (snippet) print_snippet(loc);
}

void diag_error(Loc loc, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vreport("error", C_RED, loc, fmt, ap, true);
    va_end(ap);
    if (++g_errors >= 50) diag_fatal("too many errors; aborting");
}
void diag_warning(Loc loc, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vreport("warning", C_YEL, loc, fmt, ap, true);
    va_end(ap);
    g_warnings++;
}
void diag_note(Loc loc, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vreport("note", C_CYAN, loc, fmt, ap, true);
    va_end(ap);
}
void diag_fatal(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "%sadac: fatal error:%s ", C_RED, C_RST);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(2);
}
