/* MuLIB graphical extension: glx-surface. C11, header-only. */
#ifndef MULIB_GLX_SURFACE_H
#define MULIB_GLX_SURFACE_H
#include "mulib-glx-core.h"
static inline int mulib_glx_surface_create(mgx_surface *s, int w, int h)
{
    return mgx_surface_init(s, w, h);
}

/* Pitch and capacity are bytes. Padding after the last active row is optional.
 * Borrow/view never owns storage; the owner must outlive all views and commands.
 * Output objects must be empty. Failures leave outputs unchanged.
 */
static inline int mulib_glx_surface_borrow_capacity(mgx_surface *s,
    uint32_t *pixels, size_t capacity, size_t pitch, int w, int h)
{
    return pitch % sizeof(uint32_t) == 0 &&
           mgx_wrap_capacity(s, pixels, capacity / sizeof(uint32_t),
                             pitch / sizeof(uint32_t), w, h);
}

static inline int mulib_glx_surface_borrow(mgx_surface *s, uint32_t *pixels,
                                          size_t pitch, int w, int h)
{
    return pitch % sizeof(uint32_t) == 0 &&
           mgx_wrap(s, pixels, pitch / sizeof(uint32_t), w, h);
}

static inline void mulib_glx_surface_release(mgx_surface *s)
{
    mgx_surface_free(s);
}

/* The requested rectangle is clipped to storage, not the source drawing clip.
 * A view has its own full local clip. out == s is rejected to retain ownership.
 */
static inline int mulib_glx_surface_view(mgx_surface *out, mgx_surface *s,
                                        mgx_rect r)
{
    size_t words, offset;
    if (!out || out == s || !mgx_surface_valid(s))
        return 0;
    r = mgx_intersect(r, (mgx_rect){0, 0, s->w, s->h});
    if (!r.w || !r.h)
        return 0;
    if (!mgx_surface_required_words(s->stride, s->w, s->h, &words))
        return 0;
    offset = (size_t)r.y * s->stride + (size_t)r.x;
    return mgx_wrap_capacity(out, s->pixels + offset, words - offset,
                             s->stride, r.w, r.h);
}

/* Conservative overlap detection includes inter-row padding, never compares
 * unrelated C pointers relationally, and accepts distinct borrowed view bases.
 */
static inline int mulib_glx_surface_overlaps(const mgx_surface *a,
                                            const mgx_surface *b)
{
    size_t na, nb;
    uintptr_t pa, pb;
    if (!mgx_surface_valid(a) || !mgx_surface_valid(b))
        return 0;
    if (!mgx_surface_required_words(a->stride, a->w, a->h, &na) ||
        !mgx_surface_required_words(b->stride, b->w, b->h, &nb))
        return 0;
    pa = (uintptr_t)a->pixels;
    pb = (uintptr_t)b->pixels;
    return pa < pb + nb * sizeof(uint32_t) &&
           pb < pa + na * sizeof(uint32_t);
}

/* Explicit owned copy for deferred consumers. Ignores padding, retains clip.
 * Do not copy the resulting owning struct or reinitialize a live destination.
 */
static inline int mulib_glx_surface_snapshot(mgx_surface *out,
                                            const mgx_surface *s)
{
    mgx_surface t = {0};
    if (!out || out == s || !mgx_surface_valid(s) ||
        !mgx_surface_init(&t, s->w, s->h))
        return 0;
    for (int y = 0; y < s->h; y++)
        memcpy(t.pixels + (size_t)y * t.stride,
               s->pixels + (size_t)y * s->stride,
               (size_t)s->w * sizeof(uint32_t));
    t.clip = s->clip;
    *out = t;
    return 1;
}
#endif
