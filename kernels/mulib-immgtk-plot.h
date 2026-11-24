/** @file mulib-immgtk-plot.h
 * @brief Engineering plot autoscaling bounds and logarithmic axis transformation helpers.
 * @details C11 single-header implementation; define MULIB_IMPL_IMMGTK_PLOT in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_IMMGTK_PLOT_H
#define MULIB_IMMGTK_PLOT_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
int mulib_immgtk_plot_bounds(const double *x, const double *y, size_t count, double *xmin,
                             double *xmax, double *ymin, double *ymax);
int mulib_immgtk_plot_log_transform(double x, double base, double *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMMGTK_PLOT_H */
#if defined(MULIB_IMPL_IMMGTK_PLOT) && !defined(MULIB_IMMGTK_PLOT_H_IMPLEMENTED)
#define MULIB_IMMGTK_PLOT_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_immgtk_plot_bounds(const double *x, const double *y, size_t n, double *xmin, double *xmax,
                             double *ymin, double *ymax) {
    if (!x || !y || !n || !xmin || !xmax || !ymin || !ymax)
        return 0;
    double ax = x[0], bx = ax, ay = y[0], by = ay;
    if (!isfinite(ax) || !isfinite(ay))
        return 0;
    for (size_t i = 1; i < n; i++) {
        if (!isfinite(x[i]) || !isfinite(y[i]))
            return 0;
        if (x[i] < ax)
            ax = x[i];
        if (x[i] > bx)
            bx = x[i];
        if (y[i] < ay)
            ay = y[i];
        if (y[i] > by)
            by = y[i];
    }
    *xmin = ax;
    *xmax = bx;
    *ymin = ay;
    *ymax = by;
    return 1;
}
int mulib_immgtk_plot_log_transform(double x, double base, double *out) {
    if (!out || !isfinite(x) || !isfinite(base) || x <= 0 || base <= 0 || base == 1)
        return 0;
    double v = log(x) / log(base);
    if (!isfinite(v))
        return 0;
    *out = v;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_IMMGTK_PLOT */
