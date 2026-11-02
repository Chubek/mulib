/** @file mulib-fontdraw.h
 * @brief FreeType font loading, outline extraction and glyph rasterization.
 * Define MULIB_IMPL_FONTDRAW and MULIB_IMPL_DYNLOAD once. Runtime-loaded
 * FreeType 2 uses its stable public C ABI. See manual for pixel conventions.
 */
#ifndef MULIB_FONTDRAW_H
#define MULIB_FONTDRAW_H
#include "mulib-dynload.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_fontdraw mulib_fontdraw_t;
/** @brief Owned top-down bitmap. pixel_mode follows FT_Pixel_Mode: 1 mono,
 * 2 gray, 3 gray2, 4 gray4, 5 LCD, 6 LCD_V, 7 premultiplied BGRA.
 * width/height are native sample dimensions; stride is positive bytes.
 * left/top locate bitmap relative to baseline; advances are 26.6 pixels. */
typedef struct mulib_fontdraw_bitmap {
    unsigned char *pixels;
    size_t width, height, stride;
    unsigned pixel_mode, grays;
    int left, top;
    long advance_x, advance_y;
} mulib_fontdraw_bitmap_t;
typedef struct mulib_fontdraw_point { long x, y; unsigned char tag; } mulib_fontdraw_point_t;
/** @brief Owned contour endpoints (inclusive) and points in FreeType units.
 * tag&3: 1 on-curve, 0 quadratic control, 2 cubic control. Default 26.6 pixels;
 * FT_LOAD_NO_SCALE selects font units. flags retain FT_Outline flags. */
typedef struct mulib_fontdraw_outline {
    mulib_fontdraw_point_t *points;
    size_t *contours;
    size_t point_count, contour_count;
    int flags;
} mulib_fontdraw_outline_t;
/** @brief Load a face (including TTC index) and set positive pixel height.
 * NULL runtime path selects platform FreeType name. Returns NULL on failure. */
mulib_fontdraw_t *mulib_fontdraw_open(const char *path, long index, unsigned pixels,
                                    const char *library_path);
/** @brief Release face and library; copied bitmaps/outlines remain valid. */
void mulib_fontdraw_close(mulib_fontdraw_t *font);
/** @brief Change pixel height; 0/-1. Contexts require caller synchronization. */
int mulib_fontdraw_size(mulib_fontdraw_t *font, unsigned pixels);
/** @brief Select Unicode charmap and map scalar to glyph id; 0 means
 * missing/invalid/no Unicode charmap. Legacy fonts remain usable by glyph id. */
unsigned mulib_fontdraw_glyph(mulib_fontdraw_t *font, uint32_t scalar);
/** @brief Load/render glyph using native FT_LOAD_* flags and FT_Render_Mode
 * (0 normal, 1 light, 2 mono, 3 LCD, 4 LCD_V, 5 SDF). 0/-1, output must be
 * zero-initialized. FT_LOAD_RENDER is removed so render_mode is honored. */
int mulib_fontdraw_render(mulib_fontdraw_t *font, unsigned glyph, int32_t load_flags,
                          int render_mode, mulib_fontdraw_bitmap_t *bitmap);
/** @brief Extract scalable outline; bitmap-only glyphs fail. 0/-1, output empty. */
int mulib_fontdraw_outline(mulib_fontdraw_t *font, unsigned glyph, int32_t load_flags,
                           mulib_fontdraw_outline_t *outline);
/** @brief Release and zero a bitmap. */
void mulib_fontdraw_bitmap_destroy(mulib_fontdraw_bitmap_t *bitmap);
/** @brief Release and zero an outline. */
void mulib_fontdraw_outline_destroy(mulib_fontdraw_outline_t *outline);
/** @brief Convert mono/gray/gray2/gray4/BGRA to malloc-owned width*height alpha.
 * LCD modes fail (require a subpixel compositor). 0/-1, empty gives NULL. */
int mulib_fontdraw_alpha(const mulib_fontdraw_bitmap_t *bitmap, unsigned char **alpha);
/** @brief Borrow FT_Face for complete native APIs (variations, kerning, palettes,
 * transforms, font formats, etc.). Never release it; lifetime ends at close. */
void *mulib_fontdraw_native(mulib_fontdraw_t *font);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FONTDRAW) && !defined(MULIB_FONTDRAW_IMPLEMENTED)
#define MULIB_FONTDRAW_IMPLEMENTED
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
/* ABI declarations, independently expressed from the documented FreeType 2
 * public structure layouts; no implementation code is vendored. */
typedef struct mulib_fontdraw_generic_ { void *data; void (*finalizer)(void *); } mulib_fontdraw_generic_;
typedef struct mulib_fontdraw_vector_ { long x, y; } mulib_fontdraw_vector_;
typedef struct mulib_fontdraw_native_bitmap_ {
    unsigned rows, width; int pitch; unsigned char *buffer;
    unsigned short grays; unsigned char mode, palette_mode; void *palette;
} mulib_fontdraw_native_bitmap_;
typedef struct mulib_fontdraw_native_outline_ {
    short contours, points; mulib_fontdraw_vector_ *coordinates;
    unsigned char *tags; short *ends; int flags;
} mulib_fontdraw_native_outline_;
typedef struct mulib_fontdraw_face_prefix_ {
    long faces, index, flags, style_flags, glyphs;
    char *family, *style;
    int strike_count; void *strikes;
    int charmap_count; void *charmaps;
    mulib_fontdraw_generic_ generic;
    long bbox[4];
    unsigned short upem;
    short ascender, descender, height, max_width, max_height, underline, thickness;
    void *glyph;
} mulib_fontdraw_face_prefix_;
typedef struct mulib_fontdraw_slot_prefix_ {
    void *library, *face, *next; unsigned index;
    mulib_fontdraw_generic_ generic;
    long metrics[8], linear_x, linear_y;
    mulib_fontdraw_vector_ advance;
    unsigned format;
    mulib_fontdraw_native_bitmap_ bitmap;
    int left, top;
    mulib_fontdraw_native_outline_ outline;
} mulib_fontdraw_slot_prefix_;
struct mulib_fontdraw {
    mulib_dynload_t loader;
    void *library, *face;
    int (*init)(void **);
    int (*done)(void *);
    int (*new_face)(void *, const char *, long, void **);
    int (*done_face)(void *);
    int (*set_size)(void *, unsigned, unsigned);
    int (*select_charmap)(void *, unsigned);
    unsigned (*char_index)(void *, unsigned long);
    int (*load)(void *, unsigned, int32_t);
    int (*render)(void *, int);
};
void mulib_fontdraw_close(mulib_fontdraw_t *f) {
    if (!f) return;
    if (f->face && f->done_face) (void)f->done_face(f->face);
    if (f->library && f->done) (void)f->done(f->library);
    (void)mulib_dynload_close(&f->loader, NULL, 0); free(f);
}
mulib_fontdraw_t *mulib_fontdraw_open(const char *path, long index, unsigned pixels, const char *library) {
    mulib_fontdraw_t *f;
    void *address;
    if (!path || index < 0 || !pixels) return NULL;
    f = (mulib_fontdraw_t *)calloc(1, sizeof *f);
    if (!f) return NULL;
#ifdef _WIN32
    if (!library) library = "libfreetype-6.dll";
#elif defined(__APPLE__)
    if (!library) library = "libfreetype.6.dylib";
#else
    if (!library) library = "libfreetype.so.6";
#endif
    if (mulib_dynload_open(&f->loader, library, NULL, 0)) goto fail;
#define MULIB_FONTDRAW_LOAD_(field, name) do { \
    if (sizeof f->field != sizeof address || mulib_dynload_symbol(&f->loader, name, &address, NULL, 0)) goto fail; \
    memcpy(&f->field, &address, sizeof f->field); } while (0)
    MULIB_FONTDRAW_LOAD_(init, "FT_Init_FreeType");
    MULIB_FONTDRAW_LOAD_(done, "FT_Done_FreeType");
    MULIB_FONTDRAW_LOAD_(new_face, "FT_New_Face");
    MULIB_FONTDRAW_LOAD_(done_face, "FT_Done_Face");
    MULIB_FONTDRAW_LOAD_(set_size, "FT_Set_Pixel_Sizes");
    MULIB_FONTDRAW_LOAD_(select_charmap, "FT_Select_Charmap");
    MULIB_FONTDRAW_LOAD_(char_index, "FT_Get_Char_Index");
    MULIB_FONTDRAW_LOAD_(load, "FT_Load_Glyph");
    MULIB_FONTDRAW_LOAD_(render, "FT_Render_Glyph");
#undef MULIB_FONTDRAW_LOAD_
    if (f->init(&f->library) || f->new_face(f->library, path, index, &f->face) ||
        f->set_size(f->face, 0, pixels)) goto fail;
    /* Prefer Unicode; fonts with legacy charmaps remain usable by glyph id. */
    (void)f->select_charmap(f->face, UINT32_C(0x756e6963));
    return f;
fail:
    mulib_fontdraw_close(f); return NULL;
}
void *mulib_fontdraw_native(mulib_fontdraw_t *f) { return f ? f->face : NULL; }
int mulib_fontdraw_size(mulib_fontdraw_t *f, unsigned pixels) {
    return f && pixels && !f->set_size(f->face, 0, pixels) ? 0 : -1;
}
unsigned mulib_fontdraw_glyph(mulib_fontdraw_t *f, uint32_t cp) {
    if (!f || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff) ||
        f->select_charmap(f->face, UINT32_C(0x756e6963))) return 0;
    return f->char_index(f->face, cp);
}
void mulib_fontdraw_bitmap_destroy(mulib_fontdraw_bitmap_t *b) {
    if (b) { free(b->pixels); memset(b, 0, sizeof *b); }
}
void mulib_fontdraw_outline_destroy(mulib_fontdraw_outline_t *o) {
    if (o) { free(o->points); free(o->contours); memset(o, 0, sizeof *o); }
}
int mulib_fontdraw_render(mulib_fontdraw_t *f, unsigned glyph, int32_t flags, int mode, mulib_fontdraw_bitmap_t *out) {
    mulib_fontdraw_face_prefix_ face;
    mulib_fontdraw_slot_prefix_ slot;
    mulib_fontdraw_bitmap_t b = {0};
    size_t y, pitch, minimum;
    unsigned bits;
    if (!f || !out || out->pixels || out->width || out->height || mode < 0 || mode > 5 ||
        f->load(f->face, glyph, flags & ~INT32_C(4))) return -1;
    memcpy(&face, f->face, sizeof face);
    if (!face.glyph || f->render(face.glyph, mode)) return -1;
    memcpy(&slot, face.glyph, sizeof slot);
    if (slot.bitmap.pitch == INT_MIN) return -1;
    pitch = (size_t)(slot.bitmap.pitch < 0 ? -slot.bitmap.pitch : slot.bitmap.pitch);
    b.width = slot.bitmap.width; b.height = slot.bitmap.rows; b.stride = pitch;
    b.pixel_mode = slot.bitmap.mode; b.grays = slot.bitmap.grays;
    b.left = slot.left; b.top = slot.top;
    b.advance_x = slot.advance.x; b.advance_y = slot.advance.y;
    bits = b.pixel_mode == 1 ? 1 : b.pixel_mode == 3 ? 2 : b.pixel_mode == 4 ? 4 :
           b.pixel_mode == 7 ? 32 : b.pixel_mode == 2 || b.pixel_mode == 5 || b.pixel_mode == 6 ? 8 : 0;
    if (b.width && b.height) {
        if (!bits || b.width > (SIZE_MAX - 7) / bits) return -1;
        minimum = (b.width*bits + 7)/8;
        if (pitch < minimum) return -1;
    }
    if (b.height && pitch) {
        if (pitch > SIZE_MAX / b.height || !slot.bitmap.buffer) return -1;
        b.pixels = (unsigned char *)malloc(pitch * b.height);
        if (!b.pixels) return -1;
        for (y = 0; y < b.height; ++y) {
            size_t source = slot.bitmap.pitch < 0 ? b.height - 1 - y : y;
            memcpy(b.pixels + y * pitch, slot.bitmap.buffer + source * pitch, pitch);
        }
    }
    *out = b; return 0;
}
int mulib_fontdraw_outline(mulib_fontdraw_t *f, unsigned glyph, int32_t flags, mulib_fontdraw_outline_t *out) {
    mulib_fontdraw_face_prefix_ face;
    mulib_fontdraw_slot_prefix_ slot;
    mulib_fontdraw_outline_t o = {0};
    size_t i;
    if (!f || !out || out->points || out->contours || out->point_count || out->contour_count ||
        f->load(f->face, glyph, (flags & ~INT32_C(4)) | INT32_C(8))) return -1;
    memcpy(&face, f->face, sizeof face);
    if (!face.glyph) return -1;
    memcpy(&slot, face.glyph, sizeof slot);
    if (slot.format != UINT32_C(0x6f75746c) || slot.outline.points < 0 || slot.outline.contours < 0) return -1;
    o.point_count = (size_t)slot.outline.points; o.contour_count = (size_t)slot.outline.contours;
    o.flags = slot.outline.flags;
    if ((o.point_count && (!slot.outline.coordinates || !slot.outline.tags)) ||
        (o.contour_count && !slot.outline.ends) ||
        (!o.point_count != !o.contour_count) ||
        o.point_count > SIZE_MAX / sizeof *o.points || o.contour_count > SIZE_MAX / sizeof *o.contours) return -1;
    if (o.point_count) {
        o.points = (mulib_fontdraw_point_t *)malloc(o.point_count * sizeof *o.points);
        if (!o.points) goto fail;
    }
    if (o.contour_count) {
        o.contours = (size_t *)malloc(o.contour_count * sizeof *o.contours);
        if (!o.contours) goto fail;
    }
    for (i = 0; i < o.point_count; ++i) {
        mulib_fontdraw_vector_ point;
        memcpy(&point, (const unsigned char *)slot.outline.coordinates + i*sizeof point, sizeof point);
        o.points[i].x = point.x; o.points[i].y = point.y;
        o.points[i].tag = slot.outline.tags[i];
    }
    for (i = 0; i < o.contour_count; ++i) {
        if (slot.outline.ends[i] < 0 || (size_t)slot.outline.ends[i] >= o.point_count ||
            (i && (size_t)slot.outline.ends[i] <= o.contours[i - 1])) goto fail;
        o.contours[i] = (size_t)slot.outline.ends[i];
    }
    if (o.contour_count && o.contours[o.contour_count-1] != o.point_count-1) goto fail;
    *out = o; return 0;
fail:
    mulib_fontdraw_outline_destroy(&o); return -1;
}
int mulib_fontdraw_alpha(const mulib_fontdraw_bitmap_t *b, unsigned char **out) {
    unsigned char *a;
    size_t x, y, need;
    unsigned bits, value, max;
    if (!b || !out || (b->height && b->width > SIZE_MAX / b->height)) return -1;
    bits = b->pixel_mode == 1 ? 1 : b->pixel_mode == 2 ? 8 : b->pixel_mode == 3 ? 2 :
           b->pixel_mode == 4 ? 4 : b->pixel_mode == 7 ? 32 : 0;
    if (!bits || b->width > (SIZE_MAX - 7) / bits) return -1;
    need = (b->width * bits + 7) / 8;
    if (b->stride < need || (b->height && b->stride > SIZE_MAX / b->height) ||
        (b->width && b->height && !b->pixels) || (bits == 8 && (b->grays < 2 || b->grays > 256))) return -1;
    if (!b->width || !b->height) { *out = NULL; return 0; }
    a = (unsigned char *)malloc(b->width * b->height);
    if (!a) return -1;
    max = bits == 8 ? b->grays - 1 : bits == 32 ? 255 : (1u << bits) - 1;
    for (y = 0; y < b->height; ++y) for (x = 0; x < b->width; ++x) {
        if (bits == 32) value = b->pixels[y * b->stride + 4 * x + 3];
        else if (bits == 8) value = b->pixels[y * b->stride + x];
        else value = (b->pixels[y * b->stride + x * bits / 8] >> (8 - bits - x * bits % 8)) & max;
        if (value > max) value = max;
        a[y * b->width + x] = (unsigned char)((value * 255 + max / 2) / max);
    }
    *out = a; return 0;
}
#ifdef __cplusplus
}
#endif
#endif
