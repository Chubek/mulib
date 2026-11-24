/* MuLIB graphical extension: glx-compositor. C11, header-only. */
#ifndef MULIB_GLX_COMPOSITOR_H
#define MULIB_GLX_COMPOSITOR_H
#include "mulib-glx-blend.h"
#include "mulib-glx-surface.h"

/* Source storage/clip is borrowed only for this call; source clip is ignored.
 * Finite opacity is clamped to [0,1]. All storage aliases use a source snapshot.
 * Validation/allocation failures leave the destination unchanged.
 */
static inline int mulib_glx_composite_checked(mgx_surface *d,
    const mgx_surface *s, int x, int y, double opacity, mulib_glx_blend_mode m)
{
    mgx_surface copy = {0};
    mgx_rect r;
    if (!mgx_surface_valid(d) || !mgx_surface_valid(s) || !isfinite(opacity) ||
        !mulib_glx_blend_mode_valid(m))
        return 0;
    r = mgx_intersect(mgx_surface_clip(d), (mgx_rect){x, y, s->w, s->h});
    if (!r.w || !r.h)
        return 1;
    if (mulib_glx_surface_overlaps(d, s)) {
        if (!mulib_glx_surface_snapshot(&copy, s))
            return 0;
        s = &copy;
    }
    for (int j = r.y; j < r.y + r.h; j++)
        for (int i = r.x; i < r.x + r.w; i++) {
            size_t sx = (size_t)((int64_t)i - x);
            size_t sy = (size_t)((int64_t)j - y);
            uint32_t *p = d->pixels + (size_t)j * d->stride + (size_t)i;
            *p = mulib_glx_blend(mgx_alpha(s->pixels[sy * s->stride + sx],
                                          opacity), *p, m);
        }
    mgx_surface_free(&copy);
    return 1;
}

static inline void mulib_glx_composite(mgx_surface *d, const mgx_surface *s,
    int x, int y, double opacity, mulib_glx_blend_mode m)
{
    (void)mulib_glx_composite_checked(d, s, x, y, opacity, m);
}
#endif
