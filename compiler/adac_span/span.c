/*
 * span.c - Loading source files.
 */
#include "adac_span/span.h"

Source *source_from_string(const char *name, const char *text, bool is_prelude) {
    Source *s = xcalloc(sizeof(Source));
    s->name = xstrdup(name);
    s->text = xstrdup(text);
    s->len = (int)strlen(text);
    s->is_prelude = is_prelude;
    return s;
}

Source *source_from_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = xcalloc((size_t)sz + 1);
    if (sz > 0 && fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        return NULL;
    }
    fclose(f);
    Source *s = xcalloc(sizeof(Source));
    s->name = xstrdup(path);
    s->text = buf;
    s->len = (int)sz;
    /* skip the UTF-8 BOM */
    if (s->len >= 3 && (uint8_t)buf[0] == 0xEF && (uint8_t)buf[1] == 0xBB && (uint8_t)buf[2] == 0xBF) {
        s->text += 3;
        s->len -= 3;
    }
    return s;
}

Source *source_from_stdin(const char *name) {
    size_t cap = 4096, n = 0;
    char *buf = xcalloc(cap);
    size_t got;
    while ((got = fread(buf + n, 1, cap - n - 1, stdin)) > 0) {
        n += got;
        if (n + 1 >= cap) { cap *= 2; buf = xrealloc(buf, cap); }
    }
    buf[n] = 0;
    Source *s = xcalloc(sizeof(Source));
    s->name = xstrdup(name);
    s->text = buf;
    s->len = (int)n;
    if (s->len >= 3 && (uint8_t)buf[0] == 0xEF && (uint8_t)buf[1] == 0xBB && (uint8_t)buf[2] == 0xBF) {
        s->text += 3;
        s->len -= 3;
    }
    return s;
}
