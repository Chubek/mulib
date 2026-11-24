/** @file mulib-spice-lexer.h
 * @brief SPICE engineering numeric suffixes (T,G,MEG,K,M,U,N,P,F) and bounded whitespace-line
 * tokenization for basic component records.
 * @details C11 single-header implementation; define MULIB_IMPL_SPICE_LEXER in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_SPICE_LEXER_H
#define MULIB_SPICE_LEXER_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_parse_number(const char *text, double *out);
int mulib_spice_split_line(const char *line, char fields[][64], size_t max_fields,
                           size_t *out_count);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SPICE_LEXER_H */
#if defined(MULIB_IMPL_SPICE_LEXER) && !defined(MULIB_SPICE_LEXER_H_IMPLEMENTED)
#define MULIB_SPICE_LEXER_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_parse_number(const char *text, double *out) {
    if (!text || !out)
        return 0;
    errno = 0;
    char *end;
    double v = strtod(text, &end);
    if (end == text || !isfinite(v) || errno == ERANGE)
        return 0;
    double f = 1;
    char suff[16] = {0};
    size_t n = strlen(end);
    if (n >= sizeof suff)
        return 0;
    for (size_t i = 0; i < n; i++) {
        char c = end[i];
        suff[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    if (!strcmp(suff, "t"))
        f = 1e12;
    else if (!strcmp(suff, "g"))
        f = 1e9;
    else if (!strcmp(suff, "meg"))
        f = 1e6;
    else if (!strcmp(suff, "k"))
        f = 1e3;
    else if (!strcmp(suff, "m"))
        f = 1e-3;
    else if (!strcmp(suff, "u"))
        f = 1e-6;
    else if (!strcmp(suff, "n"))
        f = 1e-9;
    else if (!strcmp(suff, "p"))
        f = 1e-12;
    else if (!strcmp(suff, "f"))
        f = 1e-15;
    else if (*suff)
        return 0;
    double z = v * f;
    if (!isfinite(z))
        return 0;
    *out = z;
    return 1;
}
int mulib_spice_split_line(const char *s, char fields[][64], size_t cap, size_t *out) {
    if (!s || !fields || !out)
        return 0;
    size_t n = 0;
    while (*s) {
        while (*s == ' ' || *s == '\t' || *s == '\r')
            s++;
        if (!*s || *s == '\n' || *s == ';' || *s == '*')
            break;
        if (n == cap)
            return 0;
        size_t k = 0;
        while (*s && *s != ' ' && *s != '\t' && *s != '\r' && *s != '\n' && *s != ';') {
            if (k == 63)
                return 0;
            fields[n][k++] = *s++;
        }
        fields[n++][k] = 0;
        if (*s == ';')
            break;
    }
    *out = n;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_SPICE_LEXER */
