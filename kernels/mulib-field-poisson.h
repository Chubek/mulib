/** @file mulib-field-poisson.h
 * @brief Closed-form homogeneous 1D electrostatic potential and electric field with uniform free
 * charge and fixed boundary potentials.
 * @details C11 single-header implementation; define MULIB_IMPL_FIELD_POISSON in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_FIELD_POISSON_H
#define MULIB_FIELD_POISSON_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
int mulib_field_poisson_1d(double length_m, double permittivity_f_m, double charge_c_m3,
                           double phi_left, double phi_right, double x_m, double *potential_volts);
int mulib_field_poisson_field_1d(double length, double permittivity, double charge, double left,
                                 double right, double x, double *out);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_FIELD_POISSON_H */
#if defined(MULIB_IMPL_FIELD_POISSON) && !defined(MULIB_FIELD_POISSON_H_IMPLEMENTED)
#define MULIB_FIELD_POISSON_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_field_poisson_1d(double L, double eps, double rho, double a, double b, double x,
                           double *out) {
    if (!out || !isfinite(L) || L <= 0 || !isfinite(eps) || eps <= 0 || !isfinite(rho) ||
        !isfinite(a) || !isfinite(b) || !isfinite(x) || x < 0 || x > L)
        return 0;
    double y = a + (b - a) * x / L + rho * x * (L - x) / (2 * eps);
    if (!isfinite(y))
        return 0;
    *out = y;
    return 1;
}
int mulib_field_poisson_field_1d(double L, double eps, double rho, double a, double b, double x,
                                 double *out) {
    double t;
    if (!out || !mulib_field_poisson_1d(L, eps, rho, a, b, x, &t))
        return 0;
    double field = -(b - a) / L - (rho / eps) * (L * .5 - x);
    if (!isfinite(field))
        return 0;
    *out = field;
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_FIELD_POISSON */
