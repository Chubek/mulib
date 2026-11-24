/* MuLIB graphical extension: glx-textatlas. C11, header-only. */
#ifndef MULIB_GLX_TEXTATLAS_H
#define MULIB_GLX_TEXTATLAS_H
#include "mulib-glx-atlas.h"
#include "mulib-glx-font.h"
/* Writes a complete white/transparent coverage cell, independent of s->clip.
 * Padding is reserved but not written. Pixels remain valid after font_free. */
static inline int mulib_glx_textatlas_add(mulib_glx_atlas_t *a,
    mgx_surface *s, const mulib_glx_font_t *f, uint32_t cp, mgx_rect *r)
{
    mulib_glx_atlas_t next;
    mgx_rect cell;
    uint32_t glyph = mulib_glx_font_glyph(f, cp);
    if (!a || !r || !mgx_surface_valid(s) ||
        a->w > s->w || a->h > s->h || glyph == UINT32_MAX)
        return 0;
    next = *a;
    if (!mulib_glx_atlas_alloc(&next, (int)f->width, (int)f->height, &cell))
        return 0;
    for (unsigned y = 0; y < f->height; ++y)
        for (unsigned x = 0; x < f->width; ++x)
            s->pixels[((size_t)cell.y + y) * s->stride + cell.x + x] =
                mulib_glx_font_glyph_bit(f, glyph, x, y) ? 0xffffffff : 0;
    *a = next;
    *r = cell;
    return 1;
}
#endif
