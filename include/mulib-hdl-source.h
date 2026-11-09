/** @file mulib-hdl-source.h
 * @brief Borrowed HDL source bytes and byte-offset to 1-based line/column mapping.
 * @details C11 single-header implementation; define MULIB_IMPL_HDL_SOURCE in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_HDL_SOURCE_H
#define MULIB_HDL_SOURCE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_hdl_source {
    const char *text;
    size_t length;
} mulib_hdl_source_t;
int mulib_hdl_source_linecol(mulib_hdl_source_t source, size_t byte_offset, size_t *line,
                             size_t *column);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HDL_SOURCE_H */
#if defined(MULIB_IMPL_HDL_SOURCE) && !defined(MULIB_HDL_SOURCE_H_IMPLEMENTED)
#define MULIB_HDL_SOURCE_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hdl_source_linecol(mulib_hdl_source_t s, size_t index, size_t *line, size_t *column) {
    if (!s.text || !line || !column || index > s.length)
        return 0;
    size_t l = 1, c = 1;
    for (size_t i = 0; i < index; i++) {
        if (s.text[i] == '\n') {
            l++;
            c = 1;
        } else
            c++;
    }
    *line = l;
    *column = c;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_HDL_SOURCE */
