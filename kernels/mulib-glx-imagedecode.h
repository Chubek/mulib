/* MuLIB graphical extension: glx-imagedecode. C11, header-only. */
#ifndef MULIB_GLX_IMAGEDECODE_H
#define MULIB_GLX_IMAGEDECODE_H
#include "mulib-glx-font.h"
#ifdef MULIB_GLX_IMAGEDECODE_ENABLE_PNG
#include "mulib-imagepng.h"
#endif
/* Native P6 PPM and BI_RGB 24/32-bit BMP; opt-in native PNG through the existing
 * imagepng/deflate/typecore kernels. Other formats return 0.
 * All limits are inclusive; a zero field forbids decoding. */
typedef struct {
    size_t max_input_bytes, max_pixels, max_output_bytes;
    uint32_t max_width, max_height;
} mulib_glx_image_limits_t;

static inline mulib_glx_image_limits_t mulib_glx_image_limits_default(void)
{
    return (mulib_glx_image_limits_t){
        (size_t)128 * 1024 * 1024, (size_t)16 * 1024 * 1024,
        (size_t)64 * 1024 * 1024, 16384, 16384
    };
}

static inline int mgx_image_space_(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
           c == '\v' || c == '\f';
}

/* Retained helper signature. Header tokens use ASCII whitespace, not binary
 * control bytes. The cursor/output are changed only after a complete token. */
static inline int mgx_ppm_token(const unsigned char *p, size_t n,
                                size_t *i, unsigned *out)
{
    size_t at, start;
    unsigned v = 0;
    if (!p || !i || !out || *i > n)
        return 0;
    at = *i;
    for (;;) {
        while (at < n && mgx_image_space_(p[at]))
            ++at;
        if (at < n && p[at] == '#') {
            while (at < n && p[at] != '\n' && p[at] != '\r')
                ++at;
        } else {
            break;
        }
    }
    start = at;
    while (at < n && p[at] >= '0' && p[at] <= '9') {
        unsigned d = p[at++] - '0';
        if (v > (UINT_MAX - d) / 10)
            return 0;
        v = v * 10 + d;
    }
    if (at == start ||
        (at < n && !mgx_image_space_(p[at]) && p[at] != '#'))
        return 0;
    *i = at;
    *out = v;
    return 1;
}

static inline int mgx_image_dimensions_(uint32_t w, uint32_t h,
    const mulib_glx_image_limits_t *limits, size_t *pixels)
{
    size_t count;
    if (!w || !h || w > INT_MAX || h > INT_MAX ||
        w > limits->max_width || h > limits->max_height ||
        (size_t)w > SIZE_MAX / h)
        return 0;
    count = (size_t)w * h;
    if (count > limits->max_pixels ||
        count > limits->max_output_bytes / sizeof(uint32_t))
        return 0;
    *pixels = count;
    return 1;
}

/* out must be a zero/empty surface. Input is borrowed only during this call.
 * Success transfers an owned surface to out; failure leaves out unchanged.
 * Explicit limits may raise the defaults but cannot bypass overflow checks. */
static inline int mulib_glx_imagedecode_limited(const void *data, size_t n,
    mgx_surface *out, const mulib_glx_image_limits_t *limits)
{
    const unsigned char *p = data;
    mulib_glx_image_limits_t defaults = mulib_glx_image_limits_default();
    mgx_surface s = {0};
    size_t pixels;
    if (!limits)
        limits = &defaults;
    if (!p || !out || out->pixels || n < 2 ||
        n > limits->max_input_bytes)
        return 0;
    if (p[0] == 'P' && p[1] == '6') {
        size_t i = 2, sample_bytes, raster_bytes;
        unsigned w, h, maximum;
        if (i == n || !mgx_image_space_(p[i]) ||
            !mgx_ppm_token(p, n, &i, &w) ||
            !mgx_ppm_token(p, n, &i, &h) ||
            !mgx_ppm_token(p, n, &i, &maximum) ||
            !maximum || maximum > 65535 || i == n ||
            !mgx_image_space_(p[i]) ||
            !mgx_image_dimensions_(w, h, limits, &pixels))
            return 0;
        /* Exactly one separator follows maxval. Do not skip raster bytes
         * that happen to be whitespace or '#'. 16-bit PPM is big-endian. */
        ++i;
        sample_bytes = maximum > 255 ? 2 : 1;
        if (pixels > SIZE_MAX / (3 * sample_bytes))
            return 0;
        raster_bytes = pixels * 3 * sample_bytes;
        if (raster_bytes > n - i ||
            !mgx_surface_init(&s, (int)w, (int)h))
            return 0;
        for (size_t k = 0; k < pixels; ++k) {
            uint32_t color = 0xff000000;
            for (unsigned channel = 0; channel < 3; ++channel) {
                unsigned v = p[i++];
                if (sample_bytes == 2)
                    v = v * 256 + p[i++];
                if (v > maximum) {
                    mgx_surface_free(&s);
                    return 0;
                }
                v = (v * 255u + maximum / 2) / maximum;
                color |= (uint32_t)v << (16 - channel * 8);
            }
            s.pixels[k] = color;
        }
    }
#ifdef MULIB_GLX_IMAGEDECODE_ENABLE_PNG
    else if (n >= 8 && !memcmp(p, "\x89PNG\r\n\x1a\n", 8)) {
        mulib_imagepng_limits_t png_limits = mulib_imagepng_limits_default();
        mulib_imagepng_rgba_t rgba = {0};
        /* Retain the PNG decoder's independent temporary-memory/chunk ceilings.
         * Raising shared image limits does not silently remove those limits. */
        png_limits.max_input_bytes = limits->max_input_bytes;
        png_limits.max_pixels = limits->max_pixels;
        png_limits.max_output_bytes = limits->max_output_bytes;
        png_limits.max_width = limits->max_width < INT_MAX
            ? limits->max_width : INT_MAX;
        png_limits.max_height = limits->max_height < INT_MAX
            ? limits->max_height : INT_MAX;
        if (!mulib_imagepng_decode_rgba(p, n, &rgba, &png_limits))
            return 0;
        if (!mgx_surface_init(&s, (int)rgba.width, (int)rgba.height)) {
            mulib_imagepng_rgba_free(&rgba);
            return 0;
        }
        for (uint32_t y = 0; y < rgba.height; ++y) {
            const unsigned char *row = rgba.pixels + (size_t)y * rgba.stride;
            for (uint32_t x = 0; x < rgba.width; ++x) {
                const unsigned char *q = row + (size_t)x * 4;
                s.pixels[(size_t)y * s.stride + x] = (uint32_t)q[3] << 24
                    | (uint32_t)q[0] << 16 | (uint32_t)q[1] << 8 | q[2];
            }
        }
        mulib_imagepng_rgba_free(&rgba);
    }
#endif
    else if (p[0] == 'B' && p[1] == 'M') {
        uint32_t off, dib, w, raw_height, h, declared, image_bytes;
        size_t row, pixel_bytes, raster_bytes;
        unsigned bpp;
        int top_down;
        if (n < 54)
            return 0;
        declared = mgx_le32(p + 2);
        off = mgx_le32(p + 10);
        dib = mgx_le32(p + 14);
        w = mgx_le32(p + 18);
        raw_height = mgx_le32(p + 22);
        top_down = (raw_height & UINT32_C(0x80000000)) != 0;
        h = top_down ? 0u - raw_height : raw_height;
        bpp = (unsigned)p[28] | (unsigned)p[29] << 8;
        image_bytes = mgx_le32(p + 34);
        if ((dib != 40 && dib != 52 && dib != 56 &&
             dib != 108 && dib != 124) ||
            dib > n - 14 || off < (size_t)14 + dib || off > n ||
            !declared || declared > n || declared < off ||
            p[26] != 1 || p[27] || mgx_le32(p + 30) ||
            (bpp != 24 && bpp != 32) ||
            !mgx_image_dimensions_(w, h, limits, &pixels))
            return 0;
        pixel_bytes = bpp / 8;
        if ((size_t)w > (SIZE_MAX - 3) / pixel_bytes)
            return 0;
        row = ((size_t)w * pixel_bytes + 3) & ~(size_t)3;
        if (row > (n - off) / h || row > (declared - off) / h)
            return 0;
        raster_bytes = row * h;
        if ((image_bytes && (image_bytes < raster_bytes ||
                             image_bytes > declared - off)) ||
            !mgx_surface_init(&s, (int)w, (int)h))
            return 0;
        for (uint32_t y = 0; y < h; ++y) {
            const unsigned char *src =
                p + off + (size_t)(top_down ? y : h - y - 1) * row;
            for (uint32_t x = 0; x < w; ++x) {
                const unsigned char *q = src + (size_t)x * pixel_bytes;
                /* BI_RGB's fourth byte is reserved, not reliable alpha. */
                s.pixels[(size_t)y * w + x] = 0xff000000 |
                    (uint32_t)q[2] << 16 | (uint32_t)q[1] << 8 | q[0];
            }
        }
    } else {
        return 0;
    }
    *out = s;
    return 1;
}

static inline int mulib_glx_imagedecode(const void *data, size_t n,
                                       mgx_surface *out)
{
    return mulib_glx_imagedecode_limited(data, n, out, NULL);
}
#endif
