/** @file mulib-fontshape.h
 * @brief HarfBuzz shaping and an opt-in, bounded native Latin OpenType backend.
 * Define MULIB_IMPL_FONTSHAPE, DYNLOAD and UTF8 implementation switches.
 * Default shaping is runtime-loaded; the opt-in native subset needs only libc.
 */
#ifndef MULIB_FONTSHAPE_H
#define MULIB_FONTSHAPE_H
#include "mulib-dynload.h"
#include "mulib-utf8.h"
#if defined(MULIB_FONTSHAPE_ENABLE_NATIVE) || defined(MULIB_FONTSHAPE_NATIVE_ONLY)
#include "mulib-sfnt.h"
#include "mulib-fontcmap.h"
#include "mulib-fontgsub.h"
#include "mulib-fontgpos.h"
#include "mulib-fontgdef.h"
#include "mulib-fontmetrics.h"
#include "mulib-fontkern.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_fontshape mulib_fontshape_t;
/** @brief Glyph id, UTF-8 byte cluster and positions in caller-selected units.
 * Positive y is up. Positions are HarfBuzz integer font-scale units. */
typedef struct mulib_fontshape_glyph {
    uint32_t glyph, cluster;
    int32_t x_advance, y_advance, x_offset, y_offset;
} mulib_fontshape_glyph_t;
typedef struct mulib_fontshape_run { mulib_fontshape_glyph_t *glyphs; size_t count; } mulib_fontshape_run_t;
typedef struct mulib_fontshape_options {
    const char *direction; /* NULL guesses; ltr, rtl, ttb, btt */
    const char *script;    /* NULL guesses; four-letter ISO 15924 */
    const char *language; /* NULL uses und */
    const char *const *features; /* HarfBuzz feature syntax, byte cluster ranges */
    size_t feature_count;
} mulib_fontshape_options_t;
/** @brief Load face from a file; index selects a collection face. scale>0 sets
 * both axes (use pixels*64 for 26.6). NULL library path uses native name.
 * Returns NULL on invalid font, unavailable backend or allocation failure. */
mulib_fontshape_t *mulib_fontshape_open(const char *path, unsigned index, int scale,
                                       const char *library_path);
/** @brief Explicit native ASCII/Latin backend: nominal cmap, liga and kern.
 * Requires ENABLE_NATIVE (or NATIVE_ONLY) and the documented dependency switches.
 * Memory input is copied. No runtime library is loaded. Unsupported shaping
 * options/scripts/lookups fail rather than silently claiming complex shaping. */
mulib_fontshape_t *mulib_fontshape_open_native(const char *path,unsigned index,int scale);
mulib_fontshape_t *mulib_fontshape_open_memory(const void *data,size_t size,unsigned index,int scale);
/** @brief Destroy font and runtime handle. */
void mulib_fontshape_close(mulib_fontshape_t *font);
/** @brief Set variable-font axes using strings such as "wght=700". Replaces
 * previous settings; zero count resets defaults. 0/-1, transactional parsing. */
int mulib_fontshape_variations(mulib_fontshape_t *font, const char *const *axes, size_t count);
/** @brief Shape one uniformly directed/script run. Strict UTF-8, including NULs.
 * Bidi/script/fallback itemization belongs to the caller. Options NULL guesses
 * segment properties. Output must be empty. 0/-1; output unchanged on failure. */
int mulib_fontshape_shape(mulib_fontshape_t *font, const char *text, size_t length,
                         const mulib_fontshape_options_t *options, mulib_fontshape_run_t *run);
/** @brief Free glyphs and clear run. */
void mulib_fontshape_run_destroy(mulib_fontshape_run_t *run);
/** @brief Borrow hb_font_t for advanced native APIs, valid until close. */
void *mulib_fontshape_native(mulib_fontshape_t *font);
/** @brief Test Unicode nominal-glyph coverage, returning 1/0 or -1 for an
 * invalid scalar/font. Variation selectors and shaping substitutions are not
 * tested by nominal coverage; callers should test complete shaped clusters. */
int mulib_fontshape_covers(mulib_fontshape_t *font, uint32_t scalar);
/** @brief Horizontal font metrics in the configured scale; descender is usually
 * negative. 0/-1, output unchanged if the font has no horizontal metrics. */
typedef struct mulib_fontshape_metrics { int32_t ascender, descender, line_gap; } mulib_fontshape_metrics_t;
int mulib_fontshape_metrics(mulib_fontshape_t *font, mulib_fontshape_metrics_t *metrics);
/** @brief Glyph ink extents, y up, in configured scale. 0/-1. */
typedef struct mulib_fontshape_extents { int32_t x_bearing, y_bearing, width, height; } mulib_fontshape_extents_t;
int mulib_fontshape_extents(mulib_fontshape_t *font, uint32_t glyph, mulib_fontshape_extents_t *extents);
/** @brief Return the positive scale selected at open, or zero for NULL. */
int mulib_fontshape_scale(const mulib_fontshape_t *font);
/** @brief Copy OpenType GDEF ligature caret positions in configured scale.
 * direction must be ltr/rtl/ttb/btt. Returns 0/-1, owned array (possibly NULL).
 * A font without caret data succeeds with count zero; no synthetic positions. */
int mulib_fontshape_carets(mulib_fontshape_t *font, uint32_t glyph, const char *direction,
                            int32_t **positions, size_t *count);
/** @brief Release caret storage using the configured allocator (NULL allowed). */
void mulib_fontshape_carets_destroy(int32_t *positions);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FONTSHAPE) && !defined(MULIB_FONTSHAPE_IMPLEMENTED)
#define MULIB_FONTSHAPE_IMPLEMENTED
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifndef MULIB_FONTSHAPE_MALLOC
#define MULIB_FONTSHAPE_MALLOC malloc
#endif
#ifndef MULIB_FONTSHAPE_CALLOC
#define MULIB_FONTSHAPE_CALLOC calloc
#endif
#ifndef MULIB_FONTSHAPE_FREE
#define MULIB_FONTSHAPE_FREE free
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* Stable public HarfBuzz value ABIs; opaque objects are never inspected. */
typedef struct mulib_fontshape_info_ { uint32_t codepoint, mask, cluster, var1, var2; } mulib_fontshape_info_;
typedef struct mulib_fontshape_pos_ { int32_t xa, ya, xo, yo; uint32_t var; } mulib_fontshape_pos_;
typedef struct mulib_fontshape_feature_ { uint32_t tag, value; unsigned start, end; } mulib_fontshape_feature_;
typedef struct mulib_fontshape_variation_ { uint32_t tag; float value; } mulib_fontshape_variation_;
struct mulib_fontshape {
    void *native_backend;
    mulib_dynload_t library;
    void *font;
    int font_scale;
    int (*metrics)(void *, void *);
    int (*extents)(void *, uint32_t, void *);
    unsigned (*carets)(void *, int, uint32_t, unsigned, unsigned *, int32_t *);
    void *(*blob_open)(const char *);
    void (*blob_destroy)(void *);
    unsigned (*face_count)(void *);
    void *(*face_create)(void *, unsigned);
    unsigned (*face_glyph_count)(const void *);
    void (*face_destroy)(void *);
    void *(*font_create)(void *);
    void *(*font_empty)(void);
    void (*font_destroy)(void *);
    void (*font_funcs)(void *);
    void (*scale)(void *, int, int);
    int (*nominal)(void *, uint32_t, uint32_t *);
    void *(*buffer_create)(void);
    void (*buffer_destroy)(void *);
    int (*buffer_ok)(void *);
    void (*add)(void *, const char *, int, unsigned, int);
    int (*direction_parse)(const char *, int);
    uint32_t (*script_parse)(const char *, int);
    const void *(*language_parse)(const char *, int);
    void (*direction)(void *, int);
    void (*script)(void *, uint32_t);
    void (*language)(void *, const void *);
    void (*guess)(void *);
    int (*feature)(const char *, int, mulib_fontshape_feature_ *);
    int (*variation)(const char *, int, mulib_fontshape_variation_ *);
    void (*variations)(void *, const mulib_fontshape_variation_ *, unsigned);
    int (*shape)(void *, void *, const mulib_fontshape_feature_ *, unsigned, const char *const *);
    mulib_fontshape_info_ *(*infos)(void *, unsigned *);
    mulib_fontshape_pos_ *(*positions)(void *, unsigned *);
};
#if defined(MULIB_FONTSHAPE_ENABLE_NATIVE) || defined(MULIB_FONTSHAPE_NATIVE_ONLY)
#include "detail/mulib-fontshape-native.h"
#else
mulib_fontshape_t *mulib_fontshape_open_native(const char *path,unsigned index,int scale) {
    (void)path;(void)index;(void)scale;return NULL;
}
mulib_fontshape_t *mulib_fontshape_open_memory(const void *data,size_t size,unsigned index,int scale) {
    (void)data;(void)size;(void)index;(void)scale;return NULL;
}
#endif
void mulib_fontshape_close(mulib_fontshape_t *f) {
    if (!f) return;
#if defined(MULIB_FONTSHAPE_ENABLE_NATIVE) || defined(MULIB_FONTSHAPE_NATIVE_ONLY)
    if(f->native_backend)mulib_fontshape_native_close_(f);
#endif
    if (f->font && f->font_destroy) f->font_destroy(f->font);
#ifndef MULIB_FONTSHAPE_NATIVE_ONLY
    (void)mulib_dynload_close(&f->library, NULL, 0);
#endif
    MULIB_FONTSHAPE_FREE(f);
}
mulib_fontshape_t *mulib_fontshape_open(const char *path, unsigned index, int scale, const char *library) {
#ifdef MULIB_FONTSHAPE_NATIVE_ONLY
    return library?NULL:mulib_fontshape_open_native(path,index,scale);
#else
    mulib_fontshape_t *f;
    void *address, *blob = NULL, *face = NULL;
    if (!path || scale <= 0) return NULL;
    f = (mulib_fontshape_t *)MULIB_FONTSHAPE_CALLOC(1, sizeof *f);
    if (!f) return NULL;
#ifdef _WIN32
    if (!library) library = "libharfbuzz-0.dll";
#elif defined(__APPLE__)
    if (!library) library = "libharfbuzz.0.dylib";
#else
    if (!library) library = "libharfbuzz.so.0";
#endif
    if (mulib_dynload_open(&f->library, library, NULL, 0)) goto fail;
#define MULIB_FONTSHAPE_LOAD_(field, name) do { \
    if (sizeof f->field != sizeof address || mulib_dynload_symbol(&f->library, name, &address, NULL, 0)) goto fail; \
    memcpy(&f->field, &address, sizeof f->field); } while (0)
    MULIB_FONTSHAPE_LOAD_(blob_open, "hb_blob_create_from_file_or_fail");
    MULIB_FONTSHAPE_LOAD_(blob_destroy, "hb_blob_destroy");
    MULIB_FONTSHAPE_LOAD_(face_count, "hb_face_count");
    MULIB_FONTSHAPE_LOAD_(face_create, "hb_face_create");
    MULIB_FONTSHAPE_LOAD_(face_glyph_count, "hb_face_get_glyph_count");
    MULIB_FONTSHAPE_LOAD_(face_destroy, "hb_face_destroy");
    MULIB_FONTSHAPE_LOAD_(font_create, "hb_font_create");
    MULIB_FONTSHAPE_LOAD_(font_empty, "hb_font_get_empty");
    MULIB_FONTSHAPE_LOAD_(font_destroy, "hb_font_destroy");
    MULIB_FONTSHAPE_LOAD_(font_funcs, "hb_ot_font_set_funcs");
    MULIB_FONTSHAPE_LOAD_(scale, "hb_font_set_scale");
    MULIB_FONTSHAPE_LOAD_(nominal, "hb_font_get_nominal_glyph");
    MULIB_FONTSHAPE_LOAD_(metrics, "hb_font_get_h_extents");
    MULIB_FONTSHAPE_LOAD_(extents, "hb_font_get_glyph_extents");
    MULIB_FONTSHAPE_LOAD_(carets, "hb_ot_layout_get_ligature_carets");
    MULIB_FONTSHAPE_LOAD_(buffer_create, "hb_buffer_create");
    MULIB_FONTSHAPE_LOAD_(buffer_destroy, "hb_buffer_destroy");
    MULIB_FONTSHAPE_LOAD_(buffer_ok, "hb_buffer_allocation_successful");
    MULIB_FONTSHAPE_LOAD_(add, "hb_buffer_add_utf8");
    MULIB_FONTSHAPE_LOAD_(direction_parse, "hb_direction_from_string");
    MULIB_FONTSHAPE_LOAD_(script_parse, "hb_script_from_string");
    MULIB_FONTSHAPE_LOAD_(language_parse, "hb_language_from_string");
    MULIB_FONTSHAPE_LOAD_(direction, "hb_buffer_set_direction");
    MULIB_FONTSHAPE_LOAD_(script, "hb_buffer_set_script");
    MULIB_FONTSHAPE_LOAD_(language, "hb_buffer_set_language");
    MULIB_FONTSHAPE_LOAD_(guess, "hb_buffer_guess_segment_properties");
    MULIB_FONTSHAPE_LOAD_(feature, "hb_feature_from_string");
    MULIB_FONTSHAPE_LOAD_(variation, "hb_variation_from_string");
    MULIB_FONTSHAPE_LOAD_(variations, "hb_font_set_variations");
    MULIB_FONTSHAPE_LOAD_(shape, "hb_shape_full");
    MULIB_FONTSHAPE_LOAD_(infos, "hb_buffer_get_glyph_infos");
    MULIB_FONTSHAPE_LOAD_(positions, "hb_buffer_get_glyph_positions");
#undef MULIB_FONTSHAPE_LOAD_
    blob = f->blob_open(path);
    if (!blob || (index & 0xffffu) >= f->face_count(blob)) goto fail;
    face = f->face_create(blob, index);
    if (!face || !f->face_glyph_count(face)) goto fail;
    f->font = f->font_create(face);
    if (!f->font || f->font == f->font_empty()) goto fail;
    f->font_funcs(f->font); f->scale(f->font, scale, scale); f->font_scale = scale;
    f->face_destroy(face); f->blob_destroy(blob); return f;
fail:
    if (face) f->face_destroy(face);
    if (blob) f->blob_destroy(blob);
    mulib_fontshape_close(f); return NULL;
#endif
}
void *mulib_fontshape_native(mulib_fontshape_t *f) { return f ? f->font : NULL; }
int mulib_fontshape_scale(const mulib_fontshape_t *f) { return f ? f->font_scale : 0; }
int mulib_fontshape_metrics(mulib_fontshape_t *f, mulib_fontshape_metrics_t *out) {
    /* hb_font_extents_t includes nine reserved positions in its public ABI. */
    int32_t m[12] = {0};
#if defined(MULIB_FONTSHAPE_ENABLE_NATIVE) || defined(MULIB_FONTSHAPE_NATIVE_ONLY)
    if(f&&f->native_backend)return mulib_fontshape_native_metrics_(f,out);
#endif
    if (!f || !out || !f->metrics(f->font, m)) return -1;
    out->ascender = m[0]; out->descender = m[1]; out->line_gap = m[2]; return 0;
}
int mulib_fontshape_extents(mulib_fontshape_t *f, uint32_t glyph, mulib_fontshape_extents_t *out) {
    int32_t e[4];
#if defined(MULIB_FONTSHAPE_ENABLE_NATIVE) || defined(MULIB_FONTSHAPE_NATIVE_ONLY)
    if(f&&f->native_backend)return mulib_fontshape_native_extents_(f,glyph,out);
#endif
    if (!f || !out || !f->extents(f->font, glyph, e)) return -1;
    out->x_bearing = e[0]; out->y_bearing = e[1]; out->width = e[2]; out->height = e[3]; return 0;
}
int mulib_fontshape_carets(mulib_fontshape_t *f, uint32_t glyph, const char *dir,
                          int32_t **out, size_t *count) {
    unsigned n = 0, capacity;
    int direction;
    int32_t *p = NULL;
    if (!f || !dir || !out || !count ||
        (strcmp(dir, "ltr") && strcmp(dir, "rtl") && strcmp(dir, "ttb") && strcmp(dir, "btt"))) return -1;
#if defined(MULIB_FONTSHAPE_ENABLE_NATIVE) || defined(MULIB_FONTSHAPE_NATIVE_ONLY)
    if(f->native_backend)return mulib_fontshape_native_carets_(f,glyph,dir,out,count);
#endif
    direction = f->direction_parse(dir, -1);
    n = f->carets(f->font, direction, glyph, 0, &n, NULL);
    if (n) {
        size_t bytes = (size_t)n * sizeof *p;
        if (bytes / sizeof *p != n || !(p = (int32_t *)MULIB_FONTSHAPE_MALLOC(bytes))) return -1;
        capacity = n;
        (void)f->carets(f->font, direction, glyph, 0, &capacity, p);
        if (capacity > n) { MULIB_FONTSHAPE_FREE(p); return -1; }
        n = capacity;
    }
    *out = p; *count = n; return 0;
}
int mulib_fontshape_covers(mulib_fontshape_t *f, uint32_t cp) {
    uint32_t glyph;
    if (!f || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return -1;
#if defined(MULIB_FONTSHAPE_ENABLE_NATIVE) || defined(MULIB_FONTSHAPE_NATIVE_ONLY)
    if(f->native_backend)return mulib_fontshape_native_covers_(f,cp);
#endif
    return f->nominal(f->font, cp, &glyph) && glyph ? 1 : 0;
}
void mulib_fontshape_carets_destroy(int32_t *positions) { MULIB_FONTSHAPE_FREE(positions); }
void mulib_fontshape_run_destroy(mulib_fontshape_run_t *r) {
    if (r) { MULIB_FONTSHAPE_FREE(r->glyphs); memset(r, 0, sizeof *r); }
}
int mulib_fontshape_variations(mulib_fontshape_t *f, const char *const *axes, size_t n) {
    mulib_fontshape_variation_ *v = NULL;
    size_t i;
    if (!f || (!axes && n) || n > UINT_MAX || n > SIZE_MAX / sizeof *v) return -1;
#if defined(MULIB_FONTSHAPE_ENABLE_NATIVE) || defined(MULIB_FONTSHAPE_NATIVE_ONLY)
    if(f->native_backend)return n?-1:0;
#endif
    if (n) {
        v = (mulib_fontshape_variation_ *)MULIB_FONTSHAPE_MALLOC(n * sizeof *v);
        if (!v) return -1;
    }
    for (i = 0; i < n; ++i)
        if (!axes[i] || !f->variation(axes[i], -1, &v[i]) || !isfinite(v[i].value)) { MULIB_FONTSHAPE_FREE(v); return -1; }
    f->variations(f->font, v, (unsigned)n); MULIB_FONTSHAPE_FREE(v); return 0;
}
int mulib_fontshape_shape(mulib_fontshape_t *f, const char *s, size_t n,
                         const mulib_fontshape_options_t *o, mulib_fontshape_run_t *out) {
#if defined(MULIB_FONTSHAPE_ENABLE_NATIVE) || defined(MULIB_FONTSHAPE_NATIVE_ONLY)
    if(f&&f->native_backend)return mulib_fontshape_native_shape_(f,s,n,o,out);
#endif
#ifdef MULIB_FONTSHAPE_NATIVE_ONLY
    (void)s;(void)n;(void)o;(void)out;return -1;
#else
    void *b = NULL;
    mulib_fontshape_feature_ *features = NULL;
    mulib_fontshape_glyph_t *glyphs = NULL;
    const unsigned char *info, *pos;
    size_t i, nf = o ? o->feature_count : 0;
    unsigned count, positions;
    int direction, result = -1;
    if (!f || !out || out->glyphs || out->count || n > INT_MAX ||
        nf > UINT_MAX || nf > SIZE_MAX / sizeof *features || (nf && !o->features) ||
        !mulib_utf8_validate((const unsigned char *)s, n)) return -1;
    if (nf) {
        features = (mulib_fontshape_feature_ *)MULIB_FONTSHAPE_MALLOC(nf * sizeof *features);
        if (!features) goto done;
    }
    for (i = 0; i < nf; ++i)
        if (!o->features[i] || !f->feature(o->features[i], -1, &features[i])) goto done;
    b = f->buffer_create();
    if (!b || !f->buffer_ok(b)) goto done;
    f->add(b, s ? s : "", (int)n, 0, (int)n);
    if (o && o->direction) {
        if (strcmp(o->direction, "ltr") && strcmp(o->direction, "rtl") &&
            strcmp(o->direction, "ttb") && strcmp(o->direction, "btt")) goto done;
        direction = f->direction_parse(o->direction, -1);
        if (!direction) goto done;
        f->direction(b, direction);
    }
    if (o && o->script) {
        if (strlen(o->script) != 4) goto done;
        for (i = 0; i < 4; ++i)
            if (!((o->script[i] >= 'a' && o->script[i] <= 'z') ||
                  (o->script[i] >= 'A' && o->script[i] <= 'Z'))) goto done;
        f->script(b, f->script_parse(o->script, -1));
    }
    f->language(b, f->language_parse(o && o->language ? o->language : "und", -1));
    f->guess(b);
    if (!f->shape(f->font, b, features, (unsigned)nf, NULL) || !f->buffer_ok(b)) goto done;
    info = (const unsigned char *)f->infos(b, &count);
    pos = (const unsigned char *)f->positions(b, &positions);
    if (count != positions || (count && (!info || !pos))) goto done;
    if (count) {
        size_t bytes = (size_t)count * sizeof *glyphs;
        if (bytes / sizeof *glyphs != count) goto done;
        glyphs = (mulib_fontshape_glyph_t *)MULIB_FONTSHAPE_MALLOC(bytes);
        if (!glyphs) goto done;
    }
    for (i = 0; i < count; ++i) {
        mulib_fontshape_info_ in;
        mulib_fontshape_pos_ position;
        /* Native objects have different effective C types. Copy ABI bytes;
         * do not dereference them as locally redeclared structure types. */
        memcpy(&in, info + i*sizeof in, sizeof in);
        memcpy(&position, pos + i*sizeof position, sizeof position);
        if (in.cluster >= n) goto done;
        glyphs[i].glyph = in.codepoint; glyphs[i].cluster = in.cluster;
        glyphs[i].x_advance = position.xa; glyphs[i].y_advance = position.ya;
        glyphs[i].x_offset = position.xo; glyphs[i].y_offset = position.yo;
    }
    out->glyphs = glyphs; out->count = count; glyphs = NULL; result = 0;
done:
    if (b) f->buffer_destroy(b);
    MULIB_FONTSHAPE_FREE(features); MULIB_FONTSHAPE_FREE(glyphs); return result;
#endif
}
#ifdef __cplusplus
}
#endif
#endif
