/* SPDX-License-Identifier: MIT */
#ifndef MULIB_NB_VIRTUALIZE_H
#define MULIB_NB_VIRTUALIZE_H
#include "mulib-nb-viewport.h"

typedef struct { size_t first, last; } mulib_nb_range;

/* Half-open range; callers skip zero extents inside it. Overscan is in document
 * units. Virtualization never changes native client lifetime. */
static inline mulib_nb_range
mulib_nb_virtualize(const mulib_nb_layout *l, const mulib_nb_viewport *v,
                     double overscan)
{
    mulib_nb_range empty = {0, 0};
    if (!l || !v || !mulib_nb_layout_current(l, l->owner)
        || !isfinite(overscan) || overscan < 0 || !isfinite(v->zoom)
        || v->zoom <= 0 || !isfinite(v->scroll) || v->scroll < 0
        || !isfinite(v->height) || v->height < 0)
        return empty;
    double total = mulib_nb_layout_prefix(l, l->n);
    double a = fmax(0, v->scroll - overscan);
    double b = fmin(total, v->scroll + v->height / v->zoom + overscan);
    if (b <= a)
        return empty;
    size_t x = mulib_nb_layout_at(l, a);
    size_t y = mulib_nb_layout_at(l, nextafter(b, -INFINITY));
    return (mulib_nb_range){x, y < l->n ? y + 1 : l->n};
}
#endif
