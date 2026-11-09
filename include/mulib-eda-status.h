/** @file mulib-eda-status.h
 * @brief Shared status names and bounded diagnostic record with line/column context.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_STATUS in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_STATUS_H
#define MULIB_EDA_STATUS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_eda_code {
    MULIB_EDA_OK = 0,
    MULIB_EDA_INVALID = -1,
    MULIB_EDA_NOMEM = -2,
    MULIB_EDA_RANGE = -3,
    MULIB_EDA_NOTFOUND = -4,
    MULIB_EDA_CONFLICT = -5,
    MULIB_EDA_SINGULAR = -6,
    MULIB_EDA_UNSUPPORTED = -7
} mulib_eda_code_t;
typedef struct mulib_eda_diagnostic {
    mulib_eda_code_t code;
    size_t offset, line, column;
    char message[192];
} mulib_eda_diagnostic_t;
const char *mulib_eda_code_name(mulib_eda_code_t code);
void mulib_eda_diagnostic_set(mulib_eda_diagnostic_t *d, mulib_eda_code_t code, size_t line,
                              size_t column, const char *msg);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_STATUS_H */
#if defined(MULIB_IMPL_EDA_STATUS) && !defined(MULIB_EDA_STATUS_H_IMPLEMENTED)
#define MULIB_EDA_STATUS_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
const char *mulib_eda_code_name(mulib_eda_code_t c) {
    switch (c) {
    case MULIB_EDA_OK:
        return "OK";
    case MULIB_EDA_INVALID:
        return "INVALID";
    case MULIB_EDA_NOMEM:
        return "NOMEM";
    case MULIB_EDA_RANGE:
        return "RANGE";
    case MULIB_EDA_NOTFOUND:
        return "NOTFOUND";
    case MULIB_EDA_CONFLICT:
        return "CONFLICT";
    case MULIB_EDA_SINGULAR:
        return "SINGULAR";
    case MULIB_EDA_UNSUPPORTED:
        return "UNSUPPORTED";
    default:
        return "UNKNOWN";
    }
}
void mulib_eda_diagnostic_set(mulib_eda_diagnostic_t *d, mulib_eda_code_t c, size_t line,
                              size_t col, const char *msg) {
    if (!d)
        return;
    d->code = c;
    d->offset = 0;
    d->line = line;
    d->column = col;
    snprintf(d->message, sizeof d->message, "%s", msg ? msg : "");
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_EDA_STATUS */
