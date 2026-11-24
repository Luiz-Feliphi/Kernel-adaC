/*
 * main.c - adaC compiler driver (adac).
 *
 *   adac programa.adac [-o saida.exe] [--check] [--tokens] [--dump-prelude]
 *
 * Pipeline:  source -> tokens -> AST -> semantic analysis -> x86-64 -> PE (.exe)
 */
#include "common.h"
#include "adac_lexer/lexer.h"
#include "adac_parse/parser.h"
#include "adac_analysis/sema.h"
#include "adac_codegen/codegen.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <time.h>
#endif

/* wall-clock time in milliseconds (high resolution) */
static double now_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
#endif
}

/* the embedded standard library (generated from library/ by src/tools/embed) */
typedef struct { const char *kind, *path, *text; } AdacLibModule;
extern const AdacLibModule ADAC_LIBRARY[];

#define VERSION "1.2.3"

static void usage(void) {
    fprintf(stderr,
        "adac %s - compiler for the adaC language (generates 64-bit Windows .exe files)\n\n"
        "usage: adac <file.adac> [options]\n"
        "  -o <file>         output executable name (default: <file>.exe)\n"
        "  --check           only check the program (does not generate the .exe)\n"
        "  --stdin-name <p>  read the program from standard input, using <p> for messages and imports (used by editors)\n"
        "  --strict          variables must be declared (error for 'x = 1' without declaration; or use #STRICT)\n"
        "  --tokens          print the tokens and exit\n"
        "  --dump-prelude    print the embedded runtime and exit (add --manual for manual-memory mode)\n"
        "  --manual          with --dump-prelude, show the #NOT_GARBAGE runtime\n"
        "  --color / --no-color\n"
        "  -L <dir>          add a directory to the library search path for `import`\n"
        "  -v                show information about the compiler stages\n"
        "  -h, --help        show this help\n", VERSION);
}

static char *default_output(const char *in) {
    const char *slash = strrchr(in, '/');
    const char *bs = strrchr(in, '\\');
    if (bs && (!slash || bs > slash)) slash = bs;
    const char *base = slash ? slash + 1 : in;
    const char *dot = strrchr(base, '.');
    size_t n = dot ? (size_t)(dot - base) : strlen(base);
    char *out = xcalloc(n + 5);
    memcpy(out, base, n);
    strcat(out, ".exe");
    return out;
}

/* ---------------- modules (import "file.adac"; / import mylib; / import std::pc;) ---------------- */
typedef struct { Source *src; TokenList toks; } Module;
static const char *stdin_name;   /* --stdin-name: the main program is read from standard input */
static Vec modules;     /* Module*, dependencies before the importer */
static Vec seen_paths;
static Vec lib_paths;   /* -L <dir> */

static bool file_exists(const char *p) {
    FILE *f = fopen(p, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

static char *norm_path(const char *p) {
    char *s = xstrdup(p);
    for (char *q = s; *q; q++) if (*q == '\\') *q = '/';
    /* strip leading "./" */
    while (s[0] == '.' && s[1] == '/') memmove(s, s + 2, strlen(s + 2) + 1);
    return s;
}

static char *dir_of(const char *path) {
    const char *s1 = strrchr(path, '/'), *s2 = strrchr(path, '\\');
    const char *sl = s1 > s2 ? s1 : s2;
    if (!sl) return xstrdup("");
    return xstrndup(path, (size_t)(sl - path + 1));
}

static char *resolve_local_import(const char *dir, const char *rel) {
    bool abs = rel[0] == '/' || rel[0] == '\\' || (rel[0] && rel[1] == ':');
    if (abs) return file_exists(rel) ? xstrdup(rel) : NULL;
    const char *bases[64];
    int nb = 0;
    bases[nb++] = dir;
    if (*dir) bases[nb++] = "";
    bases[nb++] = xsprintf("%slibs/", dir);
    bases[nb++] = xsprintf("%slib/", dir);
    if (*dir) { bases[nb++] = "libs/"; bases[nb++] = "lib/"; }
    for (int i = 0; i < lib_paths.n && nb < 60; i++) {
        const char *lp = lib_paths.v[i];
        size_t len = strlen(lp);
        bases[nb++] = (len && (lp[len - 1] == '/' || lp[len - 1] == '\\')) ? lp : xsprintf("%s/", lp);
    }
    for (int i = 0; i < nb; i++) {
        char *c1 = xsprintf("%s%s", bases[i], rel);
        if (file_exists(c1)) return c1;
        char *c2 = xsprintf("%s%s.adac", bases[i], rel);
        if (file_exists(c2)) return c2;
        char *c3 = xsprintf("%s%s/mod.adac", bases[i], rel);
        if (file_exists(c3)) return c3;
    }
    return NULL;
}

static bool is_path_tok(const Token *t);
static bool use_matches(const char *mod, const char *u);

static bool is_embedded_lib(const char *u) {
    for (const AdacLibModule *d = ADAC_LIBRARY; d->kind; d++) {
        char *mod = xstrdup(d->path);
        char *dot = strrchr(mod, '.');
        if (dot) *dot = 0;
        if (use_matches(mod, u)) return true;
    }
    return false;
}

static void load_module(const char *path) {
    char *np = norm_path(path);
    for (int i = 0; i < seen_paths.n; i++) if (!strcmp(seen_paths.v[i], np)) return;
    vec_push(&seen_paths, np);
    Module *m = xcalloc(sizeof(Module));
    m->src = (stdin_name && !strcmp(path, stdin_name)) ? source_from_stdin(path) : source_from_file(path);
    if (!m->src) diag_fatal("could not open '%s'", path);
    m->toks = lex(m->src);
    char *dir = dir_of(path);
    for (int i = 0; i + 1 < m->toks.n; i++) {
        Token *t = &m->toks.t[i];
        if (t->kind == TK_IDENT && !strcmp(t->text, "import") && m->toks.t[i + 1].kind == TK_STRING) {
            const char *rel = m->toks.t[i + 1].text;
            char *resolved = resolve_local_import(dir, rel);
            load_module(resolved ? resolved : (rel[0] == '/' || rel[0] == '\\' || (rel[0] && rel[1] == ':') ? rel : xsprintf("%s%s", dir, rel)));
        } else if (t->kind == TK_IDENT && !strcmp(t->text, "import") && is_path_tok(&m->toks.t[i + 1])) {
            if (i > 0) {
                TokKind pk = m->toks.t[i - 1].kind;
                if (pk != P_SEMI && pk != P_RBRACE && pk != TK_DIRECTIVE) continue;
            }
            char *ipath = xstrdup("");
            int j = i + 1;
            while (j < m->toks.n && is_path_tok(&m->toks.t[j])) {
                ipath = xsprintf("%s%s%s", ipath, *ipath ? "/" : "", m->toks.t[j].text);
                j++;
                if (j + 2 < m->toks.n && m->toks.t[j].kind == P_COLON && m->toks.t[j + 1].kind == P_COLON) j += 2;
                else if (j + 1 < m->toks.n && m->toks.t[j].kind == P_DOT) j += 1;
                else break;
            }
            if (!is_embedded_lib(ipath)) {
                char *resolved = resolve_local_import(dir, ipath);
                if (resolved) load_module(resolved);
            }
        }
    }
    vec_push(&modules, m);
}

/* ---------------- standard library selection (use ...;) ---------------- */
typedef struct { const AdacLibModule *def; Source *src; TokenList toks; bool selected, lexed; } LibMod;
typedef struct { char *path, *pretty; Loc loc; bool resolved; } UseReq;
static Vec libmods;      /* LibMod* */
static Vec uses;         /* UseReq* */

static void lex_lib(LibMod *lm) {
    if (lm->lexed) return;
    lm->lexed = true;
    lm->src = source_from_string(xsprintf("library/%s", lm->def->path), lm->def->text, true);
    lm->toks = lex(lm->src);
}

/* path segments may be identifiers or keywords (e.g. `use std::char;`) */
static bool is_path_tok(const Token *t) { return t->kind == TK_IDENT || (t->kind >= KW_IF && t->kind <= KW_PTR); }

/* collects `use a::b::C;` and `import a::b::C;` (or `.` separated) declarations from a token list */
static void scan_uses(TokenList *tl, const char *dir) {
    for (int i = 0; i + 1 < tl->n; i++) {
        Token *t = &tl->t[i];
        bool is_use = t->kind == TK_IDENT && !strcmp(t->text, "use");
        bool is_imp = t->kind == TK_IDENT && !strcmp(t->text, "import");
        if ((!is_use && !is_imp) || !is_path_tok(&tl->t[i + 1])) continue;
        if (i > 0) {
            TokKind pk = tl->t[i - 1].kind;
            if (pk != P_SEMI && pk != P_RBRACE && pk != TK_DIRECTIVE) continue;
        }
        char *path = xstrdup(""), *pretty = xstrdup("");
        int j = i + 1;
        while (j < tl->n && is_path_tok(&tl->t[j])) {
            path = xsprintf("%s%s%s", path, *path ? "/" : "", tl->t[j].text);
            pretty = xsprintf("%s%s%s", pretty, *pretty ? "::" : "", tl->t[j].text);
            j++;
            if (j + 2 < tl->n && tl->t[j].kind == P_COLON && tl->t[j + 1].kind == P_COLON) {
                j += 2;
                if (tl->t[j].kind == P_STAR) { j++; break; }
            } else if (j + 1 < tl->n && tl->t[j].kind == P_DOT) {
                j += 1;
                if (tl->t[j].kind == P_STAR) { j++; break; }
            } else break;
        }
        if (is_imp && !is_embedded_lib(path) && dir && resolve_local_import(dir, path)) {
            i = j;
            continue;
        }
        UseReq *u = xcalloc(sizeof(UseReq));
        u->path = path; u->pretty = pretty; u->loc = t->loc;
        vec_push(&uses, u);
        i = j;
    }
}

static bool same_name(const char *a, const char *b) {          /* ignores case and '_' */
    for (;;) {
        while (*a == '_') a++;
        while (*b == '_') b++;
        if (!*a || !*b) return !*a && !*b;
        char x = *a >= 'A' && *a <= 'Z' ? (char)(*a + 32) : *a;
        char y = *b >= 'A' && *b <= 'Z' ? (char)(*b + 32) : *b;
        if (x != y) return false;
        a++; b++;
    }
}

/* does library module `mod` (e.g. "alloc/collections/stack") satisfy `use` path `u`? */
static bool path_matches(const char *mod, const char *u) {
    size_t lu = strlen(u);
    if (!strcmp(mod, u)) return true;                                   /* use alloc::collections::stack; */
    if (!strncmp(mod, u, lu) && mod[lu] == '/') return true;            /* use alloc::collections;  (whole directory) */
    const char *us = strrchr(u, '/'), *ms = strrchr(mod, '/');
    if (us && !strncmp(mod, u, (size_t)(us - u)) && mod[us - u] == '\0') return true;   /* use alloc::boxed::Box;  (module + item) */
    if (!us || !ms) return false;
    if (us - u != ms - mod || strncmp(u, mod, (size_t)(us - u)) != 0) return false;
    return same_name(us + 1, ms + 1);                                   /* use alloc::collections::Stack; */
}

static bool use_matches(const char *mod, const char *u) {
    if (path_matches(mod, u)) return true;
    if (!strncmp(u, "std/", 4)) {                                       /* std re-exports alloc and core */
        const char *layers[] = {"alloc", "core"};
        for (int k = 0; k < 2; k++)
            if (path_matches(mod, xsprintf("%s/%s", layers[k], u + 4))) return true;
    }
    return false;
}

static void select_library(bool manual) {
    for (const AdacLibModule *d = ADAC_LIBRARY; d->kind; d++) {
        LibMod *lm = xcalloc(sizeof(LibMod));
        lm->def = d;
        lm->selected = !strcmp(d->kind, "always") || !strcmp(d->kind, manual ? "manual" : "gc");
        vec_push(&libmods, lm);
    }
    for (int i = 0; i < modules.n; i++) {
        Module *m = modules.v[i];
        scan_uses(&m->toks, dir_of(m->src->name));
    }
    for (int i = 0; i < libmods.n; i++) {
        LibMod *lm = libmods.v[i];
        if (lm->selected) { lex_lib(lm); scan_uses(&lm->toks, NULL); }
    }
    for (bool changed = true; changed;) {                               /* fixpoint: selected modules may `use` others */
        changed = false;
        for (int ui = 0; ui < uses.n; ui++) {
            UseReq *u = uses.v[ui];
            for (int i = 0; i < libmods.n; i++) {
                LibMod *lm = libmods.v[i];
                char *mod = xstrdup(lm->def->path);
                char *dot = strrchr(mod, '.');
                if (dot) *dot = 0;
                if (!use_matches(mod, u->path)) continue;
                u->resolved = true;
                if (strcmp(lm->def->kind, "opt") == 0 && !lm->selected) {
                    lm->selected = true;
                    lex_lib(lm);
                    scan_uses(&lm->toks, NULL);
                    changed = true;
                }
            }
        }
    }
}

static void report_unresolved_uses(void) {
    for (int i = 0; i < uses.n; i++) {
        UseReq *u = uses.v[i];
        if (!u->resolved) diag_error(u->loc, "unresolved import '%s' (no such library module)", u->pretty);
    }
}

static bool has_directive(TokenList *tl, const char *a, const char *b) {
    for (int i = 0; i < tl->n; i++)
        if (tl->t[i].kind == TK_DIRECTIVE && (!strcmp(tl->t[i].text, a) || !strcmp(tl->t[i].text, b))) return true;
    return false;
}

int main(int argc, char **argv) {
    double t_start = now_ms();
    const char *input = NULL, *output = NULL;
    bool check = false, tokens = false, dump = false, manual_dump = false, verbose = false;
    bool color =
#ifndef _WIN32
        isatty(2);
#else
        false;
#endif
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "-h") || !strcmp(a, "--help")) { usage(); return 0; }
        else if (!strcmp(a, "-o")) { if (++i >= argc) diag_fatal("option -o requires a file name"); output = argv[i]; }
        else if (!strcmp(a, "--check")) check = true;
        else if (!strcmp(a, "--strict")) sema_set_strict(true);
        else if (!strcmp(a, "--stdin-name")) { if (++i >= argc) diag_fatal("option --stdin-name requires a path"); stdin_name = argv[i]; input = stdin_name; }
        else if (!strcmp(a, "--tokens")) tokens = true;
        else if (!strcmp(a, "--dump-prelude")) dump = true;
        else if (!strcmp(a, "--manual")) manual_dump = true;
        else if (!strcmp(a, "--color")) color = true;
        else if (!strcmp(a, "--no-color")) color = false;
        else if (!strcmp(a, "-L") || !strcmp(a, "--lib-path")) { if (++i >= argc) diag_fatal("option %s requires a directory", a); vec_push(&lib_paths, (void *)argv[i]); }
        else if (!strcmp(a, "-v")) verbose = true;
        else if (a[0] == '-') { fprintf(stderr, "adac: unknown option '%s'\n", a); usage(); return 2; }
        else if (!input) input = a;
        else diag_fatal("more than one input file ('%s' and '%s')", input, a);
    }
    diag_set_color(color);
    if (dump) {
        for (const AdacLibModule *lm = ADAC_LIBRARY; lm->kind; lm++) {
            if (strcmp(lm->kind, "always") != 0 && strcmp(lm->kind, manual_dump ? "manual" : "gc") != 0) continue;
            printf("// ===== library/%s =====\n%s\n", lm->path, lm->text);
        }
        return 0;
    }
    if (!input) { usage(); return 2; }

    /* 1) source and lexer */
    load_module(input);
    TokenList utoks = ((Module *)modules.v[modules.n - 1])->toks;
    if (tokens) {
        for (int i = 0; i < utoks.n; i++) {
            Token *t = &utoks.t[i];
            printf("%4d:%-3d %-16s", t->loc.line, t->loc.col, token_kind_name(t->kind));
            if (t->kind == TK_IDENT || t->kind == TK_DIRECTIVE || t->kind == TK_STRING) printf(" \"%s\"", t->text);
            else if (t->kind == TK_INT || t->kind == TK_CHAR) printf(" %lld", (long long)t->ival);
            else if (t->kind == TK_FLOAT) printf(" %g", t->fval);
            putchar('\n');
        }
        return diag_error_count() ? 1 : 0;
    }

    /* 2) memory mode and runtime */
    bool manual = false;
    for (int i = 0; i < modules.n; i++) if (has_directive(&((Module *)modules.v[i])->toks, "NOT_GARBAGE", "NOT_GARGBAGE")) manual = true;
    select_library(manual);

    /* 3) parser: library first (it must always parse), then the program */
    for (int i = 0; i < libmods.n; i++) {
        LibMod *lm = libmods.v[i];
        if (lm->selected) parser_prescan(&lm->toks);
    }
    for (int i = 0; i < modules.n; i++) parser_prescan(&((Module *)modules.v[i])->toks);
    int errors_before = diag_error_count();
    Vec rt_items = {0};
    for (int i = 0; i < libmods.n; i++) {
        LibMod *lm = libmods.v[i];
        if (!lm->selected) continue;
        Vec mi = parse_program(&lm->toks);
        for (int k = 0; k < mi.n; k++) vec_push(&rt_items, mi.v[k]);
    }
    if (diag_error_count() != errors_before) diag_fatal("internal error: the standard library does not parse");
    Vec items = {0};
    for (int i = 0; i < modules.n; i++) {
        Vec mi = parse_program(&((Module *)modules.v[i])->toks);
        for (int k = 0; k < mi.n; k++) vec_push(&items, mi.v[k]);
    }
    report_unresolved_uses();
    if (verbose) fprintf(stderr, "[adac] %d top-level items; memory: %s\n", items.n, manual ? "manual (#NOT_GARBAGE)" : "garbage collector");
    if (diag_error_count()) {
        fprintf(stderr, "\n%d syntax error(s). Compilation aborted.\n", diag_error_count());
        return 1;
    }

    /* 4) semantics */
    Program *prog = sema_analyze(rt_items, items);
    if (diag_error_count()) {
        fprintf(stderr, "\n%d error(s)%s. Compilation aborted.\n", diag_error_count(),
                diag_warning_count() ? xsprintf(" and %d warning(s)", diag_warning_count()) : "");
        return 1;
    }
    if (check) {
        fprintf(stderr, "%s: ok (%d warning(s)) in %.2f ms\n", input, diag_warning_count(), now_ms() - t_start);
        return 0;
    }

    /* 5) machine code + PE */
    char *out = output ? xstrdup(output) : default_output(input);
    codegen_emit(prog, out);
    if (diag_error_count()) return 1;
    {
        FILE *f = fopen(out, "rb");
        long sz = 0;
        if (f) { fseek(f, 0, SEEK_END); sz = ftell(f); fclose(f); }
        fprintf(stderr, "compiled: %s (%ld bytes, %s memory) in %.2f ms\n", out, sz, prog->no_gc ? "manual" : "garbage-collected", now_ms() - t_start);
    }
    return 0;
}
