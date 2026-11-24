/* MuLIB graphical extension: glx-text. C11, header-only. */
#ifndef MULIB_GLX_TEXT_H
#define MULIB_GLX_TEXT_H
#include "mulib-glx-font.h"
/* Invalid UTF-8 consumes one byte and produces U+FFFD, consistently for
 * measurement and drawing. Input is length-delimited; CR is ignored. */
static inline uint32_t mgx_font_text_decode_(const char *s, size_t n,
                                            size_t *used)
{
    uint32_t cp;
    if (mgx_utf8_decode(s, n, &cp, used))
        return cp;
    *used = 1;
    return 0xfffd;
}

static inline mgx_point mulib_glx_text_measure(const mulib_glx_font_t *f,
    const char *s, size_t n, double size)
{
    mgx_point result = {0, size};
    double x = 0, advance;
    if (!mgx_font_valid_(f) || (!s && n) || !isfinite(size) ||
        size <= 0 || size > 1024)
        return (mgx_point){0, 0};
    /* Match the integral cell width used by the bitmap drawing API. */
    advance = fmax(1, floor(size * f->width / f->height));
    while (n) {
        size_t used;
        uint32_t cp = mgx_font_text_decode_(s, n, &used);
        if (cp == '\n') {
            result.x = fmax(result.x, x);
            x = 0;
            result.y += size;
        } else if (cp != '\r') {
            x += advance;
        }
        s += used;
        n -= used;
    }
    result.x = fmax(result.x, x);
    return result;
}

/* Font and text are borrowed only for the call. No queued font pointers.
 * Clip before rasterizing, and use wide pen coordinates to avoid int overflow
 * on long lines or origins near INT_MIN/INT_MAX. */
static inline void mulib_glx_text_draw(mgx_surface *s,
    const mulib_glx_font_t *f, int x, int y, const char *text, size_t n,
    int size, uint32_t color)
{
    int width;
    int64_t pen_x = x, pen_y = y, clip_right, clip_bottom;
    mgx_rect clip;
    if (!mgx_surface_valid(s) || !mgx_font_valid_(f) ||
        (!text && n) || size <= 0 || size > 1024)
        return;
    width = mgx_max(1, (int)((double)size * f->width / f->height));
    clip = mgx_surface_clip(s);
    if (!clip.w || !clip.h)
        return;
    clip_right = (int64_t)clip.x + clip.w;
    clip_bottom = (int64_t)clip.y + clip.h;
    while (n) {
        size_t used;
        uint32_t cp = mgx_font_text_decode_(text, n, &used);
        text += used;
        n -= used;
        if (cp == '\n') {
            if (pen_y > INT64_MAX - size)
                return;
            pen_x = x;
            pen_y += size;
        } else if (cp != '\r') {
            int64_t left, right, top, bottom;
            uint32_t glyph;
            if (pen_x > INT64_MAX - width || pen_y > INT64_MAX - size)
                return;
            left = pen_x > clip.x ? pen_x : clip.x;
            right = pen_x + width < clip_right ? pen_x + width : clip_right;
            top = pen_y > clip.y ? pen_y : clip.y;
            bottom = pen_y + size < clip_bottom ? pen_y + size : clip_bottom;
            glyph = mulib_glx_font_glyph(f, cp);
            for (int64_t py = top; py < bottom; ++py)
                for (int64_t px = left; px < right; ++px)
                    if (mulib_glx_font_glyph_bit(f, glyph,
                        (unsigned)((px - pen_x) * f->width / width),
                        (unsigned)((py - pen_y) * f->height / size)))
                        mgx_pixel(s, (int)px, (int)py, color);
            pen_x += width;
        }
    }
}
#endif
