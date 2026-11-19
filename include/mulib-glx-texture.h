/* MuLIB graphical extension: glx-texture. C11, header-only. */
#ifndef MULIB_GLX_TEXTURE_H
#define MULIB_GLX_TEXTURE_H
#include "mulib-glx-gradient.h"
#include "mulib-glx-surface.h"
typedef mgx_surface mulib_glx_texture_t;

/* Internal texel-coordinate sampler; caller has validated s and finite x/y.
 * Interpolate premultiplied components, then return straight ARGB, avoiding
 * transparent-color fringes. No gamma/color-space conversion is performed.
 */
static inline uint32_t mgx_texture_texel(const mgx_surface *s,
                                        double x, double y, int linear)
{
    uint32_t c[4], out;
    double weights[4], alpha = 0;
    int i, j, a, b;
    x = mgx_clamp(x, 0, s->w - 1);
    y = mgx_clamp(y, 0, s->h - 1);
    if (!linear)
        return s->pixels[(size_t)(int)(y + .5) * s->stride +
                         (size_t)(int)(x + .5)];
    i = (int)x;
    j = (int)y;
    a = mgx_min(i + 1, s->w - 1);
    b = mgx_min(j + 1, s->h - 1);
    weights[0] = (1 - (x - i)) * (1 - (y - j));
    weights[1] = (x - i) * (1 - (y - j));
    weights[2] = (1 - (x - i)) * (y - j);
    weights[3] = (x - i) * (y - j);
    c[0] = s->pixels[(size_t)j * s->stride + (size_t)i];
    c[1] = s->pixels[(size_t)j * s->stride + (size_t)a];
    c[2] = s->pixels[(size_t)b * s->stride + (size_t)i];
    c[3] = s->pixels[(size_t)b * s->stride + (size_t)a];
    for (int k = 0; k < 4; k++)
        alpha += weights[k] * (c[k] >> 24);
    if (alpha < .5)
        return 0;
    out = (uint32_t)(mgx_clamp(alpha, 0, 255) + .5) << 24;
    for (int shift = 0; shift < 24; shift += 8) {
        double value = 0;
        for (int k = 0; k < 4; k++)
            value += weights[k] * (c[k] >> 24) * mgx_ch(c[k], shift);
        out |= (uint32_t)(mgx_clamp(value / alpha, 0, 255) + .5) << shift;
    }
    return out;
}

/* Normalized endpoints select first/last texel centers. Source clip is ignored. */
static inline uint32_t mulib_glx_texture_sample(const mgx_surface *s,
                                               double u, double v, int linear)
{
    if (!mgx_surface_valid(s) || !isfinite(u) || !isfinite(v))
        return 0;
    return mgx_texture_texel(s, mgx_clamp(u, 0, 1) * (s->w - 1),
                             mgx_clamp(v, 0, 1) * (s->h - 1), linear);
}

/* Synchronous source-over draw. Aliased storage is snapshotted before writing,
 * including differently based/strided borrowed views. Allocation failure or
 * invalid arguments return 0 without writing; an empty destination returns 1.
 */
static inline int mulib_glx_texture_draw_checked(mgx_surface *d,
    const mgx_surface *s, mgx_rect dst, int linear)
{
    mgx_surface copy = {0};
    mgx_rect r;
    if (!mgx_surface_valid(d) || !mgx_surface_valid(s))
        return 0;
    if (dst.w <= 0 || dst.h <= 0)
        return 1;
    r = mgx_intersect(mgx_surface_clip(d), dst);
    if (!r.w || !r.h)
        return 1;
    if (mulib_glx_surface_overlaps(d, s)) {
        if (!mulib_glx_surface_snapshot(&copy, s))
            return 0;
        s = &copy;
    }
    for (int y = r.y; y < r.y + r.h; y++)
        for (int x = r.x; x < r.x + r.w; x++) {
            /* Edge-to-edge scaling with pixel-center alignment: an unscaled
             * draw samples exact texels even in linear mode. */
            double sx = ((double)x - dst.x + .5) * s->w / dst.w - .5;
            double sy = ((double)y - dst.y + .5) * s->h / dst.h - .5;
            mgx_pixel(d, x, y, mgx_texture_texel(s, sx, sy, linear));
        }
    mgx_surface_free(&copy);
    return 1;
}

static inline void mulib_glx_texture_draw(mgx_surface *d, const mgx_surface *s,
                                         mgx_rect dst, int linear)
{
    (void)mulib_glx_texture_draw_checked(d, s, dst, linear);
}
#endif
