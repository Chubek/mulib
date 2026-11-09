/** @file mulib-spice-stamp.h
 * @brief MNA matrix and RHS allocation and linear conductance/independent current/ideal
 * voltage-source stamps.
 * @details C11 single-header implementation; define MULIB_IMPL_SPICE_STAMP in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_SPICE_STAMP_H
#define MULIB_SPICE_STAMP_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_spice_stamp {
    size_t unknowns;
    double *a, *b;
} mulib_spice_stamp_t;
int mulib_spice_stamp_init(mulib_spice_stamp_t *s, size_t unknowns);
void mulib_spice_stamp_free(mulib_spice_stamp_t *s);
int mulib_spice_stamp_conductance(mulib_spice_stamp_t *s, uint16_t a, uint16_t b, double g);
int mulib_spice_stamp_current(mulib_spice_stamp_t *s, uint16_t a, uint16_t b, double amps);
int mulib_spice_stamp_voltage(mulib_spice_stamp_t *s, uint16_t a, uint16_t b, size_t branch,
                              double volts);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SPICE_STAMP_H */
#if defined(MULIB_IMPL_SPICE_STAMP) && !defined(MULIB_SPICE_STAMP_H_IMPLEMENTED)
#define MULIB_SPICE_STAMP_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_stamp_init(mulib_spice_stamp_t *s, size_t n) {
    if (!s || n == 0 || n > SIZE_MAX / n || n * n > SIZE_MAX / sizeof(double))
        return 0;
    memset(s, 0, sizeof *s);
    s->a = calloc(n * n, sizeof(double));
    s->b = calloc(n, sizeof(double));
    if (!s->a || !s->b) {
        mulib_spice_stamp_free(s);
        return 0;
    }
    s->unknowns = n;
    return 1;
}
void mulib_spice_stamp_free(mulib_spice_stamp_t *s) {
    if (!s)
        return;
    free(s->a);
    free(s->b);
    memset(s, 0, sizeof *s);
}
int mulib_spice_stamp_conductance(mulib_spice_stamp_t *s, uint16_t a, uint16_t b, double g) {
    if (!s || !isfinite(g) || g < 0 || a > s->unknowns || b > s->unknowns)
        return 0;
    if (a)
        s->a[(a - 1) * s->unknowns + a - 1] += g;
    if (b)
        s->a[(b - 1) * s->unknowns + b - 1] += g;
    if (a && b) {
        s->a[(a - 1) * s->unknowns + b - 1] -= g;
        s->a[(b - 1) * s->unknowns + a - 1] -= g;
    }
    return 1;
}
int mulib_spice_stamp_current(mulib_spice_stamp_t *s, uint16_t a, uint16_t b, double i) {
    if (!s || !isfinite(i) || a > s->unknowns || b > s->unknowns)
        return 0;
    if (a)
        s->b[a - 1] -= i;
    if (b)
        s->b[b - 1] += i;
    return 1;
}
int mulib_spice_stamp_voltage(mulib_spice_stamp_t *s, uint16_t a, uint16_t b, size_t k, double v) {
    if (!s || !isfinite(v) || k >= s->unknowns || a > s->unknowns || b > s->unknowns)
        return 0;
    if (a) {
        s->a[(a - 1) * s->unknowns + k] += 1;
        s->a[k * s->unknowns + a - 1] += 1;
    }
    if (b) {
        s->a[(b - 1) * s->unknowns + k] -= 1;
        s->a[k * s->unknowns + b - 1] -= 1;
    }
    s->b[k] += v;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_SPICE_STAMP */
