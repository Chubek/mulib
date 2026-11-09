/** @file mulib-num-integration.h
 * @brief Composite trapezoid and Simpson quadrature, plus adaptive trapezoids using a Richardson
 * error estimate and at most 30 refinement rounds.
 * @details C11 single-header implementation; define MULIB_IMPL_NUM_INTEGRATION in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_NUM_INTEGRATION_H
#define MULIB_NUM_INTEGRATION_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef double (*mulib_num_integrand)(double t, void *user);
int mulib_num_integrate_trapezoid(mulib_num_integrand f, void *user, double lo, double hi,
                                  size_t intervals, double *out);
int mulib_num_integrate_simpson(mulib_num_integrand f, void *user, double lo, double hi,
                                size_t intervals, double *out);
/* Composite trapezoids, doubling panels until the Richardson error estimate
 * is within absolute tolerance. Returns failure when the refinement budget ends. */
int mulib_num_integrate_adaptive(mulib_num_integrand f, void *user, double lo, double hi,
                                 double tolerance, size_t refinements, double *out, double *error);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_NUM_INTEGRATION_H */
#if defined(MULIB_IMPL_NUM_INTEGRATION) && !defined(MULIB_NUM_INTEGRATION_H_IMPLEMENTED)
#define MULIB_NUM_INTEGRATION_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_integrate_trapezoid(mulib_num_integrand f, void *u, double a, double b, size_t n,
                                  double *out) {
    if (!f || !out || !n || n == SIZE_MAX || !isfinite(a) || !isfinite(b))
        return 0;
    double h = (b - a) / n, sum = 0;
    if (!isfinite(h))
        return 0;
    for (size_t i = 0; i <= n; i++) {
        double v = f(a + h * i, u);
        if (!isfinite(v))
            return 0;
        sum += (i == 0 || i == n ? .5 : 1) * v;
    }
    sum *= h;
    if (!isfinite(sum))
        return 0;
    *out = sum;
    return 1;
}
int mulib_num_integrate_simpson(mulib_num_integrand f, void *u, double a, double b, size_t n,
                                double *out) {
    if (!f || !out || !n || n == SIZE_MAX || (n & 1) || !isfinite(a) || !isfinite(b))
        return 0;
    double h = (b - a) / n, sum = 0;
    if (!isfinite(h))
        return 0;
    for (size_t i = 0; i <= n; i++) {
        double v = f(a + h * i, u);
        if (!isfinite(v))
            return 0;
        sum += (i == 0 || i == n ? 1 : i & 1 ? 4 : 2) * v;
    }
    sum *= h / 3;
    if (!isfinite(sum))
        return 0;
    *out = sum;
    return 1;
}
int mulib_num_integrate_adaptive(mulib_num_integrand f, void *u, double a, double b, double tol,
                                 size_t refinements, double *out, double *error) {
    if (!f || !out || !error || !isfinite(a) || !isfinite(b) || !isfinite(tol) || tol <= 0 ||
        !refinements || refinements > 30)
        return 0;
    double previous;
    if (!mulib_num_integrate_trapezoid(f, u, a, b, 1, &previous))
        return 0;
    size_t n = 1;
    for (size_t i = 0; i < refinements; i++) {
        double h = (b - a) / (double)n, sum = 0;
        for (size_t k = 0; k < n; k++) {
            double v = f(a + ((double)k + .5) * h, u);
            if (!isfinite(v))
                return 0;
            sum += v;
        }
        double next = previous * .5 + (h * .5) * sum, err = fabs(next - previous) / 3;
        if (!isfinite(next) || !isfinite(err))
            return 0;
        if (err <= tol) {
            *out = next;
            *error = err;
            return 1;
        }
        previous = next;
        n *= 2;
    }
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_NUM_INTEGRATION */
