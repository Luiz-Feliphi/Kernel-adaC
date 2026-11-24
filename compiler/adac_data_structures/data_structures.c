/*
 * data_structures.c - Memory helpers and the pointer vector.
 */
#include "adac_data_structures/data_structures.h"

static void oom(void) {
    fprintf(stderr, "adac: fatal error: out of memory\n");
    exit(2);
}

void *xcalloc(size_t n) {
    void *p = calloc(1, n ? n : 1);
    if (!p) oom();
    return p;
}
void *xrealloc(void *p, size_t n) {
    p = realloc(p, n ? n : 1);
    if (!p) oom();
    return p;
}
char *xstrdup(const char *s) {
    size_t n = strlen(s);
    char *r = xcalloc(n + 1);
    memcpy(r, s, n);
    return r;
}
char *xstrndup(const char *s, size_t n) {
    char *r = xcalloc(n + 1);
    memcpy(r, s, n);
    return r;
}
char *xsprintf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    char *r = xcalloc((size_t)n + 1);
    vsnprintf(r, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return r;
}

void vec_push(Vec *vec, void *item) {
    if (vec->n == vec->cap) {
        vec->cap = vec->cap ? vec->cap * 2 : 8;
        vec->v = xrealloc(vec->v, sizeof(void *) * (size_t)vec->cap);
    }
    vec->v[vec->n++] = item;
}
