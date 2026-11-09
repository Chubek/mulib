/** @file mulib-num-rootfind.h
 * @brief Bracketed scalar bisection and damped Newton iteration with bounded residual-decreasing
 * backtracking.
 * @details C11 single-header implementation; define MULIB_IMPL_NUM_ROOTFIND in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_NUM_ROOTFIND_H
#define MULIB_NUM_ROOTFIND_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef double (*mulib_num_scalar_fn)(double x, void *user);
int mulib_num_bisect(mulib_num_scalar_fn f, void *user, double lo, double hi, double tol,
                     size_t max_iter, double *out);
int mulib_num_newton(mulib_num_scalar_fn f, mulib_num_scalar_fn deriv, void *user, double x,
                     double tol, size_t steps, double *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_NUM_ROOTFIND_H */
#if defined(MULIB_IMPL_NUM_ROOTFIND) && !defined(MULIB_NUM_ROOTFIND_H_IMPLEMENTED)
#define MULIB_NUM_ROOTFIND_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_bisect(mulib_num_scalar_fn f, void *u, double a, double b, double tol, size_t steps,
                     double *out) {
    if (!f || !out || !isfinite(a) || !isfinite(b) || a >= b || tol <= 0 || !isfinite(tol))
        return 0;
    double fa = f(a, u), fb = f(b, u);
    if (!isfinite(fa) || !isfinite(fb) || (((fa > 0) == (fb > 0)) && (fa != 0 && fb != 0)))
        return 0;
    if (fa == 0) {
        *out = a;
        return 1;
    }
    if (fb == 0) {
        *out = b;
        return 1;
    }
    for (size_t k = 0; k < steps; k++) {
        double mid = a * .5 + b * .5, fm = f(mid, u);
        if (!isfinite(fm))
            return 0;
        if (fabs(fm) <= tol || fabs(b - a) <= tol) {
            *out = mid;
            return 1;
        }
        if ((fa > 0) == (fm > 0)) {
            a = mid;
            fa = fm;
        } else {
            b = mid;
            fb = fm;
        }
    }
    if (fabs(b - a) <= tol) {
        *out = a * .5 + b * .5;
        return 1;
    }
    return 0;
}
int mulib_num_newton(mulib_num_scalar_fn f, mulib_num_scalar_fn df, void *u, double x, double tol,
                     size_t steps, double *out) {
    if (!f || !df || !out || !isfinite(x) || !isfinite(tol) || tol <= 0)
        return 0;
    double y = f(x, u);
    if (!isfinite(y))
        return 0;
    for (size_t i = 0; i < steps; i++) {
        if (fabs(y) <= tol) {
            *out = x;
            return 1;
        }
        double d = df(x, u);
        if (!isfinite(d) || d == 0)
            return 0;
        double step = y / d;
        if (!isfinite(step))
            return 0;
        int accepted = 0;
        for (unsigned j = 0; j < 64; j++) {
            double next = x - step;
            if (isfinite(next) && next != x) {
                double v = f(next, u);
                if (isfinite(v) && fabs(v) < fabs(y)) {
                    x = next;
                    y = v;
                    accepted = 1;
                    break;
                }
            }
            step *= .5;
        }
        if (!accepted)
            return 0;
    }
    if (fabs(y) > tol)
        return 0;
    *out = x;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_NUM_ROOTFIND */
