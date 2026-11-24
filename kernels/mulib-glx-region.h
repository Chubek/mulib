/* MuLIB graphical extension: glx-region. C11, header-only. */
#ifndef MULIB_GLX_REGION_H
#define MULIB_GLX_REGION_H
#include "mulib-glx-core.h"
/* A rectangle collection, not a canonical union: overlaps remain separate.
 * Nonempty rectangles must have representable pixel coordinates (exclusive
 * right/bottom may equal INT_MAX+1). Zero initialize; do not copy owning regions.
 */
typedef struct { mgx_rect *v; size_t n, cap; } mulib_glx_region_t;

static inline int mulib_glx_region_rect_valid(mgx_rect a)
{
    return a.w <= 0 || a.h <= 0 ||
           ((int64_t)a.x + a.w <= (int64_t)INT_MAX + 1 &&
            (int64_t)a.y + a.h <= (int64_t)INT_MAX + 1);
}

static inline int mulib_glx_region_valid(const mulib_glx_region_t *r)
{
    if (!r || r->n > r->cap || r->cap > SIZE_MAX / sizeof *r->v ||
        (r->cap && !r->v))
        return 0;
    for (size_t i = 0; i < r->n; i++)
        if (!mulib_glx_region_rect_valid(r->v[i]))
            return 0;
    return 1;
}

static inline int mulib_glx_region_add(mulib_glx_region_t *r, mgx_rect a)
{
    if (!r || r->n > r->cap || !mulib_glx_region_rect_valid(a))
        return 0;
    if (a.w <= 0 || a.h <= 0)
        return 1;
    if (r->n == SIZE_MAX ||
        !mgx_grow((void **)&r->v, &r->cap, r->n + 1, sizeof *r->v))
        return 0;
    r->v[r->n++] = a;
    return 1;
}

static inline void mulib_glx_region_clip(mulib_glx_region_t *r, mgx_rect a)
{
    size_t n = 0;
    if (!mulib_glx_region_valid(r))
        return;
    for (size_t i = 0; i < r->n; i++) {
        mgx_rect b = mgx_intersect(a, r->v[i]);
        if (b.w && b.h)
            r->v[n++] = b;
    }
    r->n = n;
}

/* Internal endpoint helper: do not narrow an empty piece at INT_MAX+1. */
static inline int mgx_region_piece(mulib_glx_region_t *r, int64_t x,
                                   int64_t y, int64_t w, int64_t h)
{
    if (w <= 0 || h <= 0)
        return 1;
    if (x < INT_MIN || x > INT_MAX || y < INT_MIN || y > INT_MAX ||
        w > INT_MAX || h > INT_MAX)
        return 0;
    return mulib_glx_region_add(r, (mgx_rect){(int)x, (int)y, (int)w, (int)h});
}

/* Transactional: allocation/representation failure preserves the old region. */
static inline int mulib_glx_region_subtract(mulib_glx_region_t *r, mgx_rect a)
{
    mulib_glx_region_t t = {0};
    if (!mulib_glx_region_valid(r))
        return 0;
    for (size_t i = 0; i < r->n; i++) {
        mgx_rect b = r->v[i], c = mgx_intersect(a, b);
        int64_t right = (int64_t)b.x + b.w;
        int64_t bottom = (int64_t)b.y + b.h;
        int64_t cright = (int64_t)c.x + c.w;
        int64_t cbottom = (int64_t)c.y + c.h;
        if (!c.w || !c.h) {
            if (!mulib_glx_region_add(&t, b))
                goto fail;
            continue;
        }
        if (!mgx_region_piece(&t, b.x, b.y, b.w, (int64_t)c.y - b.y) ||
            !mgx_region_piece(&t, b.x, cbottom, b.w, bottom - cbottom) ||
            !mgx_region_piece(&t, b.x, c.y, (int64_t)c.x - b.x, c.h) ||
            !mgx_region_piece(&t, cright, c.y, right - cright, c.h))
            goto fail;
    }
    free(r->v);
    *r = t;
    return 1;
fail:
    free(t.v);
    return 0;
}

static inline void mulib_glx_region_free(mulib_glx_region_t *r)
{
    if (r) {
        free(r->v);
        memset(r, 0, sizeof *r);
    }
}
#endif
