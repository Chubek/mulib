/* MuLIB graphical extension: glx-blend. C11, header-only. */
#ifndef MULIB_GLX_BLEND_H
#define MULIB_GLX_BLEND_H
#include "mulib-glx-core.h"
typedef enum {
    MULIB_GLX_OVER, MULIB_GLX_SOURCE, MULIB_GLX_MULTIPLY,
    MULIB_GLX_SCREEN, MULIB_GLX_ADD
} mulib_glx_blend_mode;
typedef mulib_glx_blend_mode mulib_glx_blend_mode_t;

static inline int mulib_glx_blend_mode_valid(mulib_glx_blend_mode mode)
{
    return mode >= MULIB_GLX_OVER && mode <= MULIB_GLX_ADD;
}

/* Separable blend followed by source-over, on straight byte components.
 * The nonoverlapping source contribution must not be multiplied by transparent
 * backdrop RGB. ADD is a saturated component blend, not Porter-Duff PLUS.
 */
static inline uint32_t mulib_glx_blend(uint32_t s, uint32_t d,
                                     mulib_glx_blend_mode mode)
{
    unsigned a = s >> 24, b = d >> 24;
    unsigned alpha = a * 255u + b * (255u - a);
    uint32_t out;
    if (mode == MULIB_GLX_SOURCE)
        return s;
    if (mode == MULIB_GLX_OVER || !mulib_glx_blend_mode_valid(mode))
        return mgx_over(s, d);
    if (!alpha)
        return 0;
    out = ((alpha + 127u) / 255u) << 24;
    for (int k = 0; k < 24; k += 8) {
        unsigned x = mgx_ch(s, k), y = mgx_ch(d, k), value;
        unsigned n;
        if (mode == MULIB_GLX_MULTIPLY)
            value = (x * y + 127u) / 255u;
        else if (mode == MULIB_GLX_SCREEN)
            value = 255u - ((255u - x) * (255u - y) + 127u) / 255u;
        else
            value = x + y > 255u ? 255u : x + y;
        n = x * a * (255u - b) + y * b * (255u - a) + value * a * b;
        out |= ((n + alpha / 2u) / alpha) << k;
    }
    return out;
}
#endif
