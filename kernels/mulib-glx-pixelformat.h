/* MuLIB graphical extension: glx-pixelformat. C11, header-only. */
#ifndef MULIB_GLX_PIXELFORMAT_H
#define MULIB_GLX_PIXELFORMAT_H
#include "mulib-glx-core.h"
/* Formats describe native-endian integer words, not byte-ordered wire formats.
 * XRGB imports/exports opaque alpha. RGB565 discards alpha, without matting.
 */
typedef enum {
    MGX_ARGB8888, MGX_XRGB8888, MGX_ABGR8888, MGX_RGB565
} mulib_glx_pixelformat_t;

static inline size_t mulib_glx_pixelformat_bytes(mulib_glx_pixelformat_t f)
{
    switch (f) {
    case MGX_ARGB8888:
    case MGX_XRGB8888:
    case MGX_ABGR8888:
        return sizeof(uint32_t);
    case MGX_RGB565:
        return sizeof(uint16_t);
    }
    return 0;
}

static inline uint32_t mulib_glx_pixelformat_pack(uint32_t c,
                                                mulib_glx_pixelformat_t f)
{
    switch (f) {
    case MGX_XRGB8888:
        return c | 0xff000000u;
    case MGX_ABGR8888:
        return (c & 0xff00ff00u) | ((c & 255u) << 16) | ((c >> 16) & 255u);
    case MGX_RGB565:
        return (((c >> 19) & 31u) << 11) |
               (((c >> 10) & 63u) << 5) | ((c >> 3) & 31u);
    default:
        return c;
    }
}

static inline uint32_t mulib_glx_pixelformat_unpack(uint32_t c,
                                                  mulib_glx_pixelformat_t f)
{
    if (f == MGX_RGB565)
        return 0xff000000u | ((((c >> 11) & 31u) * 255u / 31u) << 16) |
               ((((c >> 5) & 63u) * 255u / 63u) << 8) | ((c & 31u) * 255u / 31u);
    return mulib_glx_pixelformat_pack(c, f);
}

static inline int mgx_pixelformat_span(size_t pitch, size_t bpp, int w, int h,
                                      size_t *span)
{
    size_t row;
    if (!span || !bpp || w <= 0 || h <= 0 || (size_t)w > SIZE_MAX / bpp)
        return 0;
    row = (size_t)w * bpp;
    if (pitch < row || ((size_t)h - 1 &&
        pitch > (SIZE_MAX - row) / ((size_t)h - 1)))
        return 0;
    *span = ((size_t)h - 1) * pitch + row;
    return 1;
}

/* Byte capacities/strides, unaligned buffers allowed. Padding is untouched.
 * Actual accessible sizes must match the supplied capacities. Overlap snapshots
 * the active source rows first, even for in-place conversions of different bpp.
 * All validation/allocation failures return 0 without changing the destination.
 */
static inline int mulib_glx_pixelformat_convert(
    void *dst, size_t dst_capacity, size_t dst_pitch, mulib_glx_pixelformat_t df,
    const void *src, size_t src_capacity, size_t src_pitch,
    mulib_glx_pixelformat_t sf, int w, int h)
{
    size_t dbpp = mulib_glx_pixelformat_bytes(df);
    size_t sbpp = mulib_glx_pixelformat_bytes(sf);
    size_t ds, ss, row;
    unsigned char *copy = NULL, *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    uintptr_t dp = (uintptr_t)dst, sp = (uintptr_t)src;
    if (!dst || !src || !mgx_pixelformat_span(dst_pitch, dbpp, w, h, &ds) ||
        !mgx_pixelformat_span(src_pitch, sbpp, w, h, &ss) ||
        ds > dst_capacity || ss > src_capacity ||
        ds > UINTPTR_MAX - dp || ss > UINTPTR_MAX - sp)
        return 0;
    row = (size_t)w * sbpp;
    if (dp < sp + ss && sp < dp + ds) {
        if ((size_t)h > SIZE_MAX / row)
            return 0;
        copy = (unsigned char *)malloc(row * (size_t)h);
        if (!copy)
            return 0;
        for (int y = 0; y < h; y++)
            memcpy(copy + (size_t)y * row, s + (size_t)y * src_pitch, row);
        s = copy;
        src_pitch = row;
    }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint32_t c;
            const unsigned char *p = s + (size_t)y * src_pitch + (size_t)x * sbpp;
            unsigned char *q = d + (size_t)y * dst_pitch + (size_t)x * dbpp;
            if (sbpp == sizeof(uint16_t)) {
                uint16_t packed;
                memcpy(&packed, p, sizeof packed);
                c = packed;
            } else {
                memcpy(&c, p, sizeof c);
            }
            c = mulib_glx_pixelformat_pack(mulib_glx_pixelformat_unpack(c, sf), df);
            if (dbpp == sizeof(uint16_t)) {
                uint16_t packed = (uint16_t)c;
                memcpy(q, &packed, sizeof packed);
            } else {
                memcpy(q, &c, sizeof c);
            }
        }
    free(copy);
    return 1;
}
#endif
