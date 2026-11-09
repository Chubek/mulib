/** @file mulib-eda-quantity.h
 * @brief First-order uncertainty propagation for independent unit-tagged quantities with addition,
 * multiplication and division.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_QUANTITY in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_QUANTITY_H
#define MULIB_EDA_QUANTITY_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-eda-units.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eda_quantity {
    double value, sigma;
    mulib_eda_dimension_t dim;
} mulib_eda_quantity_t;
int mulib_eda_quantity_add(mulib_eda_quantity_t a, mulib_eda_quantity_t b,
                           mulib_eda_quantity_t *out);
int mulib_eda_quantity_mul(mulib_eda_quantity_t a, mulib_eda_quantity_t b,
                           mulib_eda_quantity_t *out);
int mulib_eda_quantity_div(mulib_eda_quantity_t a, mulib_eda_quantity_t b,
                           mulib_eda_quantity_t *out);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_QUANTITY_H */
#if defined(MULIB_IMPL_EDA_QUANTITY) && !defined(MULIB_EDA_QUANTITY_H_IMPLEMENTED)
#define MULIB_EDA_QUANTITY_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_quantity_add(mulib_eda_quantity_t a, mulib_eda_quantity_t b,
                           mulib_eda_quantity_t *out) {
    if (!isfinite(a.value) || !isfinite(b.value) || !isfinite(a.sigma) || !isfinite(b.sigma) ||
        !out || !mulib_eda_dimension_equal(a.dim, b.dim) || a.sigma < 0 || b.sigma < 0)
        return 0;
    mulib_eda_quantity_t c = a;
    c.value = a.value + b.value;
    c.sigma = hypot(a.sigma, b.sigma);
    if (!isfinite(c.value) || !isfinite(c.sigma))
        return 0;
    *out = c;
    return 1;
}
int mulib_eda_quantity_mul(mulib_eda_quantity_t a, mulib_eda_quantity_t b,
                           mulib_eda_quantity_t *out) {
    if (!isfinite(a.value) || !isfinite(b.value) || !isfinite(a.sigma) || !isfinite(b.sigma) ||
        !out || a.sigma < 0 || b.sigma < 0)
        return 0;
    mulib_eda_quantity_t c = {0};
    for (int i = 0; i < 7; i++) {
        int d = a.dim.exp[i] + b.dim.exp[i];
        if (d < -127 || d > 127)
            return 0;
        c.dim.exp[i] = (int8_t)d;
    }
    c.value = a.value * b.value;
    c.sigma = hypot(b.value * a.sigma, a.value * b.sigma);
    if (!isfinite(c.value) || !isfinite(c.sigma))
        return 0;
    *out = c;
    return 1;
}
int mulib_eda_quantity_div(mulib_eda_quantity_t a, mulib_eda_quantity_t b,
                           mulib_eda_quantity_t *out) {
    if (!out || !isfinite(a.value) || !isfinite(b.value) || b.value == 0 || !isfinite(a.sigma) ||
        !isfinite(b.sigma) || a.sigma < 0 || b.sigma < 0)
        return 0;
    mulib_eda_quantity_t r = {0};
    for (size_t i = 0; i < 7; i++) {
        int d = (int)a.dim.exp[i] - b.dim.exp[i];
        if (d < INT8_MIN || d > INT8_MAX)
            return 0;
        r.dim.exp[i] = (int8_t)d;
    }
    r.value = a.value / b.value;
    r.sigma = hypot(a.sigma / b.value, r.value * (b.sigma / b.value));
    if (!isfinite(r.value) || !isfinite(r.sigma))
        return 0;
    *out = r;
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_EDA_QUANTITY */
