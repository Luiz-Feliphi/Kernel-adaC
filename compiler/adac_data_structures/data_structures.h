/*
 * data_structures.h - Basic containers and memory helpers shared by the whole compiler
 * (the counterpart of rustc_data_structures).
 */
#ifndef ADAC_DATA_STRUCTURES_H
#define ADAC_DATA_STRUCTURES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

/* Growable vector of pointers */
typedef struct Vec {
    void **v;
    int n, cap;
} Vec;

void  vec_push(Vec *vec, void *item);

/* Memory / strings (the compiler is short-lived: nothing is freed) */
void *xcalloc(size_t n);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);
char *xsprintf(const char *fmt, ...);

#endif
