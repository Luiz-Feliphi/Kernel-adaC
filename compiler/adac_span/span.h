/*
 * span.h - Source files and source locations (the counterpart of rustc_span).
 */
#ifndef ADAC_SPAN_H
#define ADAC_SPAN_H
#include "adac_data_structures/data_structures.h"

typedef struct Source {
    char *name;        /* file name (for messages) */
    char *text;        /* contents, terminated by '\0' */
    int   len;
    bool  is_prelude;  /* true for the embedded standard library */
} Source;

typedef struct Loc {
    Source *src;
    int line, col;     /* 1-based */
} Loc;

/* Returns NULL when the file cannot be read. */
Source *source_from_file(const char *path);
/* Reads the whole standard input; 'name' is used in diagnostics. */
Source *source_from_stdin(const char *name);
Source *source_from_string(const char *name, const char *text, bool is_prelude);

#endif
