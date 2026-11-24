/* MuLIB graphical extension: glx-glyphcache. C11, header-only. */
#ifndef MULIB_GLX_GLYPHCACHE_H
#define MULIB_GLX_GLYPHCACHE_H
#include "mulib-glx-font.h"
#include "mulib-glx-rendercache.h"
static inline uint64_t mulib_glx_glyphcache_key(uint64_t font,uint32_t glyph,uint32_t size){uint32_t k[2]={glyph,size};return mgx_hash(k,sizeof k,font);}
/* s must be empty; returned pixels are independent of the source font. */
static inline int mulib_glx_glyphcache_raster(mgx_surface *s,
    const mulib_glx_font_t *f, uint32_t cp, uint32_t color)
{
    mgx_surface result = {0};
    uint32_t glyph = mulib_glx_font_glyph(f, cp);
    if (!s || s->pixels || glyph == UINT32_MAX ||
        !mgx_surface_init(&result, (int)f->width, (int)f->height))
        return 0;
    for (unsigned y = 0; y < f->height; ++y)
        for (unsigned x = 0; x < f->width; ++x)
            if (mulib_glx_font_glyph_bit(f, glyph, x, y))
                result.pixels[(size_t)y * result.stride + x] = color;
    *s = result;
    return 1;
}
#endif
