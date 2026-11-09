/** @file mulib-field-thermal.h
 * @brief Closed-form homogeneous 1D steady conduction with uniform heat generation and fixed
 * boundary temperatures, plus heat flux.
 * @details C11 single-header implementation; define MULIB_IMPL_FIELD_THERMAL in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_FIELD_THERMAL_H
#define MULIB_FIELD_THERMAL_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
int mulib_field_thermal_1d(double length_m, double conductivity_w_mk, double heat_w_m3,
                           double left_kelvin, double right_kelvin, double x_m, double *out_kelvin);
int mulib_field_thermal_flux_1d(double length, double conductivity, double heat, double left,
                                double right, double x, double *out);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_FIELD_THERMAL_H */
#if defined(MULIB_IMPL_FIELD_THERMAL) && !defined(MULIB_FIELD_THERMAL_H_IMPLEMENTED)
#define MULIB_FIELD_THERMAL_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_field_thermal_1d(double L, double k, double q, double tl, double tr, double x,
                           double *out) {
    if (!out || !isfinite(L) || L <= 0 || !isfinite(k) || k <= 0 || !isfinite(q) || !isfinite(tl) ||
        !isfinite(tr) || !isfinite(x) || x < 0 || x > L)
        return 0;
    double t = tl + (tr - tl) * x / L + q * x * (L - x) / (2 * k);
    if (!isfinite(t))
        return 0;
    *out = t;
    return 1;
}
int mulib_field_thermal_flux_1d(double L, double k, double q, double a, double b, double x,
                                double *out) {
    double t;
    if (!out || !mulib_field_thermal_1d(L, k, q, a, b, x, &t))
        return 0;
    double flux = -k * ((b - a) / L) - q * (L * .5 - x);
    if (!isfinite(flux))
        return 0;
    *out = flux;
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_FIELD_THERMAL */
