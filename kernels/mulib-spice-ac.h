/** @file mulib-spice-ac.h
 * @brief Frequency-domain complex MNA for linear R/C/I/V circuits at one nonnegative frequency,
 * interpreting independent DC source magnitudes as AC phasor amplitudes of zero phase for this
 * narrow profile.
 * @details C11 single-header implementation; define MULIB_IMPL_SPICE_AC in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_SPICE_AC_H
#define MULIB_SPICE_AC_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-spice-netlist.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_spice_complex {
    double re, im;
} mulib_spice_complex_t;
int mulib_spice_ac_solve(const mulib_spice_netlist_t *net, double frequency_hz,
                         mulib_spice_complex_t *node_voltages, size_t capacity);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SPICE_AC_H */
#if defined(MULIB_IMPL_SPICE_AC) && !defined(MULIB_SPICE_AC_H_IMPLEMENTED)
#define MULIB_SPICE_AC_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <complex.h>
int mulib_spice_ac_solve(const mulib_spice_netlist_t *net, double f, mulib_spice_complex_t *out,
                         size_t cap) {
    if (!mulib_spice_netlist_valid(net) || !out || cap < net->nodes || !isfinite(f) || f < 0)
        return 0;
    size_t vs = 0;
    for (size_t i = 0; i < net->parts; i++)
        if (net->parts_data[i].kind == 'V')
            vs++;
    size_t n = net->nodes + vs;
    if (!n || n > SIZE_MAX / n || n * n > SIZE_MAX / sizeof(double complex))
        return 0;
    double complex *a = calloc(n * n, sizeof(double complex)),
                   *rhs = calloc(n, sizeof(double complex));
    if (!a || !rhs) {
        free(a);
        free(rhs);
        return 0;
    }
    int ok = 1;
    size_t branch = 0;
    for (size_t i = 0; i < net->parts; i++) {
        const mulib_spice_part_t *p = net->parts_data + i;
        size_t x = p->a ? (size_t)p->a - 1 : SIZE_MAX, y = p->b ? (size_t)p->b - 1 : SIZE_MAX;
        double complex g = 0;
        switch (p->kind) {
        case 'R':
            g = 1 / p->value;
            break;
        case 'C':
            g = I * (2 * 3.14159265358979323846 * f * p->value);
            break;
        case 'I':
            if (x != SIZE_MAX)
                rhs[x] -= p->value;
            if (y != SIZE_MAX)
                rhs[y] += p->value;
            break;
        case 'V': {
            size_t z = net->nodes + branch++;
            if (x != SIZE_MAX) {
                a[x * n + z] += 1;
                a[z * n + x] += 1;
            }
            if (y != SIZE_MAX) {
                a[y * n + z] -= 1;
                a[z * n + y] -= 1;
            }
            rhs[z] += p->value;
        } break;
        default:
            ok = 0;
            break;
        }
        if (p->kind == 'R' || p->kind == 'C') {
            if (x != SIZE_MAX)
                a[x * n + x] += g;
            if (y != SIZE_MAX)
                a[y * n + y] += g;
            if (x != SIZE_MAX && y != SIZE_MAX) {
                a[x * n + y] -= g;
                a[y * n + x] -= g;
            }
        }
    }
    for (size_t k = 0; k < n && ok; k++) {
        size_t pivot = k;
        double best = cabs(a[k * n + k]);
        for (size_t i = k + 1; i < n; i++)
            if (cabs(a[i * n + k]) > best) {
                best = cabs(a[i * n + k]);
                pivot = i;
            }
        if (!isfinite(best) || best < 1e-14) {
            ok = 0;
            break;
        }
        if (pivot != k) {
            for (size_t j = k; j < n; j++) {
                double complex t = a[k * n + j];
                a[k * n + j] = a[pivot * n + j];
                a[pivot * n + j] = t;
            }
            double complex t = rhs[k];
            rhs[k] = rhs[pivot];
            rhs[pivot] = t;
        }
        for (size_t i = k + 1; i < n; i++) {
            double complex t = a[i * n + k] / a[k * n + k];
            a[i * n + k] = 0;
            for (size_t j = k + 1; j < n; j++)
                a[i * n + j] -= t * a[k * n + j];
            rhs[i] -= t * rhs[k];
        }
    }
    if (ok) {
        for (size_t i = n; i-- > 0;) {
            double complex s = rhs[i];
            for (size_t j = i + 1; j < n; j++)
                s -= a[i * n + j] * rhs[j];
            rhs[i] = s / a[i * n + i];
            if (!isfinite(creal(rhs[i])) || !isfinite(cimag(rhs[i]))) {
                ok = 0;
                break;
            }
        }
    }
    if (ok)
        for (size_t i = 0; i < net->nodes; i++)
            out[i] = (mulib_spice_complex_t){creal(rhs[i]), cimag(rhs[i])};
    free(rhs);
    free(a);
    return ok;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_SPICE_AC */
