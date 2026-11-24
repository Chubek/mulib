/** @file mulib-eda-units.h
 * @brief SI base dimensions and a deliberately scoped engineering unit set (m, kg, s, A, K, mol,
 * cd, Hz, V, Ohm, F, H, W, C, degC, rad, deg).
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_UNITS in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_UNITS_H
#define MULIB_EDA_UNITS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eda_dimension {
    int8_t exp[7];
} mulib_eda_dimension_t;
typedef struct mulib_eda_unit {
    const char *symbol;
    mulib_eda_dimension_t dim;
    double scale, offset;
} mulib_eda_unit_t;
int mulib_eda_unit_lookup(const char *name, mulib_eda_unit_t *out);
int mulib_eda_parse_engineering(const char *text, double *out, mulib_eda_dimension_t *dim);
int mulib_eda_convert(double value, const char *from, const char *to, double *out);
int mulib_eda_dimension_equal(mulib_eda_dimension_t a, mulib_eda_dimension_t b);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_UNITS_H */
#if defined(MULIB_IMPL_EDA_UNITS) && !defined(MULIB_EDA_UNITS_H_IMPLEMENTED)
#define MULIB_EDA_UNITS_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
static const struct mulib_eda_unit_record {
    const char *symbol;
    int8_t e[7];
    double scale, offset;
} mulib_eda_units_[] = {{"1", {0, 0, 0, 0, 0, 0, 0}, 1, 0},
                        {"m", {1, 0, 0, 0, 0, 0, 0}, 1, 0},
                        {"kg", {0, 1, 0, 0, 0, 0, 0}, 1, 0},
                        {"s", {0, 0, 1, 0, 0, 0, 0}, 1, 0},
                        {"A", {0, 0, 0, 1, 0, 0, 0}, 1, 0},
                        {"K", {0, 0, 0, 0, 1, 0, 0}, 1, 0},
                        {"mol", {0, 0, 0, 0, 0, 1, 0}, 1, 0},
                        {"cd", {0, 0, 0, 0, 0, 0, 1}, 1, 0},
                        {"Hz", {0, 0, -1, 0, 0, 0, 0}, 1, 0},
                        {"V", {2, 1, -3, -1, 0, 0, 0}, 1, 0},
                        {"Ohm", {2, 1, -3, -2, 0, 0, 0}, 1, 0},
                        {"F", {-2, -1, 4, 2, 0, 0, 0}, 1, 0},
                        {"H", {2, 1, -2, -2, 0, 0, 0}, 1, 0},
                        {"W", {2, 1, -3, 0, 0, 0, 0}, 1, 0},
                        {"C", {0, 0, 1, 1, 0, 0, 0}, 1, 0},
                        {"degC", {0, 0, 0, 0, 1, 0, 0}, 1, 273.15},
                        {"rad", {0, 0, 0, 0, 0, 0, 0}, 1, 0},
                        {"deg", {0, 0, 0, 0, 0, 0, 0}, 0.017453292519943295, 0}};
int mulib_eda_dimension_equal(mulib_eda_dimension_t a, mulib_eda_dimension_t b) {
    return !memcmp(a.exp, b.exp, 7);
}
int mulib_eda_unit_lookup(const char *n, mulib_eda_unit_t *out) {
    if (!n || !out)
        return 0;
    for (size_t i = 0; i < sizeof(mulib_eda_units_) / sizeof(*mulib_eda_units_); i++)
        if (!strcmp(n, mulib_eda_units_[i].symbol)) {
            out->symbol = mulib_eda_units_[i].symbol;
            memcpy(out->dim.exp, mulib_eda_units_[i].e, 7);
            out->scale = mulib_eda_units_[i].scale;
            out->offset = mulib_eda_units_[i].offset;
            return 1;
        }
    return 0;
}
static double mulib_eda_prefix_(char c) {
    switch (c) {
    case 'T':
        return 1e12;
    case 'G':
        return 1e9;
    case 'M':
        return 1e6;
    case 'k':
        return 1e3;
    case 'm':
        return 1e-3;
    case 'u':
        return 1e-6;
    case 'n':
        return 1e-9;
    case 'p':
        return 1e-12;
    case 'f':
        return 1e-15;
    default:
        return 0;
    }
}
static int mulib_eda_unit_extended_(const char *s, mulib_eda_unit_t *u) {
    if (mulib_eda_unit_lookup(s, u))
        return 1;
    if (!s || !s[0] || !s[1])
        return 0;
    double factor = mulib_eda_prefix_(s[0]);
    if (!factor || !mulib_eda_unit_lookup(s + 1, u) || u->offset != 0)
        return 0;
    u->scale *= factor;
    return 1;
}
int mulib_eda_convert(double v, const char *from, const char *to, double *out) {
    mulib_eda_unit_t f, t;
    if (!out || !isfinite(v) || !mulib_eda_unit_extended_(from, &f) ||
        !mulib_eda_unit_extended_(to, &t) || !mulib_eda_dimension_equal(f.dim, t.dim))
        return 0;
    double z = (v * f.scale + f.offset - t.offset) / t.scale;
    if (!isfinite(z))
        return 0;
    *out = z;
    return 1;
}
int mulib_eda_parse_engineering(const char *text, double *out, mulib_eda_dimension_t *dim) {
    if (!text || !out || !dim)
        return 0;
    errno = 0;
    char *end;
    double v = strtod(text, &end);
    if (errno == ERANGE || end == text || !isfinite(v))
        return 0;
    while (*end == ' ' || *end == '\t')
        end++;
    mulib_eda_unit_t u;
    if (!*end) {
        memset(dim, 0, sizeof *dim);
        *out = v;
        return 1;
    }
    if (!mulib_eda_unit_extended_(end, &u))
        return 0;
    double z = v * u.scale + u.offset;
    if (!isfinite(z))
        return 0;
    *out = z;
    *dim = u.dim;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_EDA_UNITS */
