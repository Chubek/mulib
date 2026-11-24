/** @file mulib-glyph.h
 * @brief Paired shaping/raster fonts and renderer-neutral positioned glyphs.
 * Reuses pglayout's ICU bidi/script/grapheme fallback itemizer. See manual for
 * transitive implementation switches. Runtime font backends are optional.
 */
#ifndef MULIB_GLYPH_H
#define MULIB_GLYPH_H
#include "mulib-pglayout.h"
#include "mulib-fontdraw.h"
#include "mulib-sysfonts.h"
#include "mulib-ligature.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_glyph_font mulib_glyph_font_t;
/** @brief A glyph in visual order. Positions/advances are pixels, y down;
 * x/y are baseline origins including shaping offsets. Byte cluster range is
 * absolute in the supplied paragraph. font_index identifies the borrowed set. */
typedef struct mulib_glyph {
    uint32_t id;
    size_t start, length, font_index;
    unsigned level;
    double x, y, advance;
} mulib_glyph_t;
typedef struct mulib_glyph_run { mulib_glyph_t *glyphs; size_t count; double advance; unsigned paragraph_level; } mulib_glyph_run_t;
/** @brief Open matching HarfBuzz/FreeType faces at an integer pixel size.
 * Owns both contexts. NULL on invalid input/backend/font failure. */
mulib_glyph_font_t *mulib_glyph_font_open(const char *path, unsigned index, unsigned pixels);
/** @brief Resolve a Fontconfig pattern and open paired faces. Catalog borrowed. */
mulib_glyph_font_t *mulib_glyph_font_match(mulib_sysfonts_t *catalog, const char *pattern, unsigned pixels);
/** @brief Release paired font. Borrowed handles become invalid. */
void mulib_glyph_font_close(mulib_glyph_font_t *font);
/** @brief Borrow shaping/raster contexts; do not resize or change variations
 * independently when rendering paired output. */
mulib_fontshape_t *mulib_glyph_font_shape(mulib_glyph_font_t *font);
/** @brief Borrow FreeType context. */
mulib_fontdraw_t *mulib_glyph_font_draw(mulib_glyph_font_t *font);
/** @brief Shape a horizontal line in paragraph context; fonts use 26.6 pixels.
 * Bidi, script and whole-grapheme fallback follow pglayout_shape_line. 0/-1,
 * output zero-initialized, transactional. Results own their glyph storage. */
int mulib_glyph_shape(const mulib_pglayout_fontset_t *fonts, const char *paragraph, size_t bytes,
                      size_t start, size_t length, mulib_glyph_run_t *run);
/** @brief Release/zero glyph result. */
void mulib_glyph_run_destroy(mulib_glyph_run_t *run);
/** @brief Render a glyph id through its paired font; same output/flags contract
 * as fontdraw_render. Positioning/compositing belongs to the renderer. */
int mulib_glyph_render(mulib_glyph_font_t *font, uint32_t glyph, int32_t flags,
                       int mode, mulib_fontdraw_bitmap_t *bitmap);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_GLYPH) && !defined(MULIB_GLYPH_IMPLEMENTED)
#define MULIB_GLYPH_IMPLEMENTED
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_glyph_font { mulib_fontshape_t *shape; mulib_fontdraw_t *draw; };
void mulib_glyph_font_close(mulib_glyph_font_t *f) {
    if (f) { mulib_fontshape_close(f->shape); mulib_fontdraw_close(f->draw); free(f); }
}
mulib_glyph_font_t *mulib_glyph_font_open(const char *path, unsigned index, unsigned pixels) {
    mulib_glyph_font_t *f;
    if (!path || !pixels || pixels > INT_MAX/64 || index > INT_MAX) return NULL;
    f = (mulib_glyph_font_t *)calloc(1, sizeof *f);
    if (!f) return NULL;
    f->shape = mulib_fontshape_open(path, index, (int)pixels*64, NULL);
    f->draw = mulib_fontdraw_open(path, (long)index, pixels, NULL);
    if (!f->shape || !f->draw) { mulib_glyph_font_close(f); return NULL; }
    return f;
}
mulib_glyph_font_t *mulib_glyph_font_match(mulib_sysfonts_t *catalog, const char *pattern, unsigned pixels) {
    mulib_sysfonts_font_t record = {0};
    mulib_glyph_font_t *f;
    if (mulib_sysfonts_match(catalog, pattern, &record)) return NULL;
    f = record.index < 0 ? NULL : mulib_glyph_font_open(record.path, (unsigned)record.index, pixels);
    mulib_sysfonts_font_destroy(&record); return f;
}
mulib_fontshape_t *mulib_glyph_font_shape(mulib_glyph_font_t *f) { return f ? f->shape : NULL; }
mulib_fontdraw_t *mulib_glyph_font_draw(mulib_glyph_font_t *f) { return f ? f->draw : NULL; }
int mulib_glyph_render(mulib_glyph_font_t *f, uint32_t id, int32_t flags, int mode, mulib_fontdraw_bitmap_t *b) {
    return f ? mulib_fontdraw_render(f->draw, id, flags, mode, b) : -1;
}
void mulib_glyph_run_destroy(mulib_glyph_run_t *r) {
    if (r) { free(r->glyphs); memset(r, 0, sizeof *r); }
}
int mulib_glyph_shape(const mulib_pglayout_fontset_t *fonts, const char *s, size_t n,
                      size_t start, size_t length, mulib_glyph_run_t *out) {
    mulib_pglayout_shaped_line_t line = {0};
    mulib_glyph_run_t r = {0};
    size_t i, j, k = 0;
    double x = 0;
    if (!out || out->glyphs || out->count || mulib_pglayout_shape_line(fonts, s, n, start, length, &line)) return -1;
    for (i = 0; i < line.count; ++i) {
        if (line.runs[i].glyphs.count > SIZE_MAX / sizeof *r.glyphs - r.count) goto fail;
        r.count += line.runs[i].glyphs.count;
    }
    if (r.count && !(r.glyphs = (mulib_glyph_t *)calloc(r.count, sizeof *r.glyphs))) goto fail;
    for (i = 0; i < line.count; ++i) {
        mulib_pglayout_shaped_run_t *run = &line.runs[i];
        mulib_ligature_cluster_t *clusters = NULL;
        size_t nc = 0, ci;
        /* The temporary shaped line is ours: rebase each item so validation
         * scans only its own text, not an ever-growing paragraph prefix. */
        for (j = 0; j < run->glyphs.count; ++j) {
            if (run->glyphs.glyphs[j].cluster < run->start) goto fail;
            run->glyphs.glyphs[j].cluster -= (uint32_t)run->start;
        }
        if (mulib_ligature_clusters(s+run->start, run->length, &run->glyphs, &clusters, &nc)) goto fail;
        for (ci = 0; ci < nc; ++ci) {
            size_t a = clusters[ci].first_glyph, b = a+clusters[ci].glyph_count;
            for (j = a; j < b; ++j) {
                const mulib_fontshape_glyph_t *g = &run->glyphs.glyphs[j];
                mulib_glyph_t *v = &r.glyphs[k++];
                v->id = g->glyph; v->start = run->start+clusters[ci].start; v->length = clusters[ci].length;
                v->font_index = run->font_index; v->level = run->level;
                v->x = x + g->x_offset/64.0; v->y = -g->y_offset/64.0;
                v->advance = g->x_advance/64.0; x += v->advance;
                if (!isfinite(x) || !isfinite(v->x)) { free(clusters); goto fail; }
            }
        }
        free(clusters);
    }
    r.advance = line.advance; r.paragraph_level = line.paragraph_level;
    mulib_pglayout_shaped_line_destroy(&line); *out = r; return 0;
fail:
    mulib_glyph_run_destroy(&r); mulib_pglayout_shaped_line_destroy(&line); return -1;
}
#ifdef __cplusplus
}
#endif
#endif
