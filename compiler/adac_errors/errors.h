/*
 * errors.h - Diagnostics: errors, warnings and notes with source snippets
 * (the counterpart of rustc_errors).
 */
#ifndef ADAC_ERRORS_H
#define ADAC_ERRORS_H
#include "adac_span/span.h"

void diag_error(Loc loc, const char *fmt, ...);
void diag_warning(Loc loc, const char *fmt, ...);
void diag_note(Loc loc, const char *fmt, ...);
void diag_fatal(const char *fmt, ...);     /* error without a location, aborts */
int  diag_error_count(void);
int  diag_warning_count(void);
void diag_set_color(bool on);

#endif
