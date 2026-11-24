/* SPDX-License-Identifier: MIT */
#ifndef MULIB_NB_VIEWPORT_H
#define MULIB_NB_VIEWPORT_H
#include "mulib-nb-layout.h"

typedef struct { double scroll, zoom, width, height; } mulib_nb_viewport;

static inline int
mulib_nb_viewport_set(mulib_nb_viewport *v, double scroll, double zoom,
                      double w, double h, double total)
{
    if (!v || !isfinite(scroll) || !isfinite(zoom) || zoom <= 0
        || !isfinite(w) || !isfinite(h) || w < 0 || h < 0
        || !isfinite(total) || total < 0 || !isfinite(h / zoom))
        return 0;
    *v = (mulib_nb_viewport){
        fmax(0, fmin(scroll, fmax(0, total - h / zoom))), zoom, w, h
    };
    return 1;
}

static inline double
mulib_nb_viewport_y(const mulib_nb_viewport *v, double y)
{
    if (!v || !isfinite(y) || !isfinite(v->scroll)
        || !isfinite(v->zoom) || v->zoom <= 0)
        return NAN;
    return (y - v->scroll) * v->zoom;
}

static inline double
mulib_nb_viewport_inverse(const mulib_nb_viewport *v, double y)
{
    if (!v || !isfinite(y) || !isfinite(v->scroll)
        || !isfinite(v->zoom) || v->zoom <= 0)
        return NAN;
    return y / v->zoom + v->scroll;
}

static inline int
mulib_nb_viewport_reveal(mulib_nb_viewport *v, const mulib_nb_layout *l, size_t i)
{
    if (!v || !l || !mulib_nb_layout_current(l, l->owner) || i >= l->n
        || !isfinite(v->zoom) || v->zoom <= 0 || l->extent[i] <= 0)
        return 0;
    double a = mulib_nb_layout_prefix(l, i), b = a + l->extent[i];
    double h = v->height / v->zoom, scroll = v->scroll;
    if (a < scroll || b - a > h)
        scroll = a;
    else if (b > scroll + h)
        scroll = b - h;
    return mulib_nb_viewport_set(v, scroll, v->zoom, v->width, v->height,
                                 mulib_nb_layout_prefix(l, l->n));
}
#endif
