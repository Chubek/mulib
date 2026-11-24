/** @file mulib-ligature.h
 * @brief OpenType ligature controls and lossless UTF-8 cluster maps.
 * Define MULIB_IMPL_LIGATURE plus FONTSHAPE, UTF8 and DYNLOAD switches.
 */
#ifndef MULIB_LIGATURE_H
#define MULIB_LIGATURE_H
#include "mulib-fontshape.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Optional ligature features. Required script ligatures remain enabled. */
enum { MULIB_LIGATURE_STANDARD = 1, MULIB_LIGATURE_CONTEXTUAL = 2,
       MULIB_LIGATURE_DISCRETIONARY = 4, MULIB_LIGATURE_HISTORICAL = 8 };
/** @brief Visual-order cluster mapping; source offsets are bytes. Multiple
 * scalars in a cluster may be marks, emoji or a ligature, not necessarily GSUB. */
typedef struct mulib_ligature_cluster {
    size_t start, length, first_glyph, glyph_count, scalar_count;
} mulib_ligature_cluster_t;
/** @brief Shape with explicit liga/clig/dlig/hlig settings. Caller features are
 * applied after these defaults. Options/ranges follow fontshape. 0/-1. */
int mulib_ligature_shape(mulib_fontshape_t *font, const char *text, size_t length,
                         unsigned flags, const mulib_fontshape_options_t *options,
                         mulib_fontshape_run_t *run);
/** @brief Build an owned visual-order map from a monotone HarfBuzz run. Text is
 * the exact input to shaping; clusters must start at UTF-8 scalar boundaries.
 * 0/-1, outputs unchanged on failure. Free array with free(). Empty succeeds. */
int mulib_ligature_clusters(const char *text, size_t length, const mulib_fontshape_run_t *run,
                            mulib_ligature_cluster_t **clusters, size_t *count);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LIGATURE) && !defined(MULIB_LIGATURE_IMPLEMENTED)
#define MULIB_LIGATURE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ligature_shape(mulib_fontshape_t *f, const char *s, size_t n, unsigned flags,
                         const mulib_fontshape_options_t *options, mulib_fontshape_run_t *out) {
    mulib_fontshape_options_t o = {0};
    const char **features;
    size_t i;
    int result;
    if (flags & ~15u) return -1;
    if (options) o = *options;
    if ((o.feature_count && !o.features) || o.feature_count > SIZE_MAX / sizeof *features - 4) return -1;
    features = (const char **)malloc((o.feature_count + 4) * sizeof *features);
    if (!features) return -1;
    features[0] = flags & 1 ? "liga=1" : "liga=0";
    features[1] = flags & 2 ? "clig=1" : "clig=0";
    features[2] = flags & 4 ? "dlig=1" : "dlig=0";
    features[3] = flags & 8 ? "hlig=1" : "hlig=0";
    for (i = 0; i < o.feature_count; ++i) features[i+4] = o.features[i];
    o.features = features; o.feature_count += 4;
    result = mulib_fontshape_shape(f, s, n, &o, out); free(features); return result;
}
int mulib_ligature_clusters(const char *s, size_t n, const mulib_fontshape_run_t *r,
                            mulib_ligature_cluster_t **out, size_t *count) {
    mulib_ligature_cluster_t *c = NULL;
    size_t i, k = 0;
    int direction = 0;
    if (!r || !out || !count || (r->count && !r->glyphs) ||
        r->count > SIZE_MAX / sizeof *c || !mulib_utf8_validate((const unsigned char *)s, n)) return -1;
    if (r->count && !(c = (mulib_ligature_cluster_t *)calloc(r->count, sizeof *c))) return -1;
    for (i = 0; i < r->count; ++i) {
        size_t at = r->glyphs[i].cluster;
        if (at >= n || ((unsigned char)s[at] & 0xc0) == 0x80) goto fail;
        if (k && at == c[k-1].start) { ++c[k-1].glyph_count; continue; }
        if (k) {
            int d = at > c[k-1].start ? 1 : -1;
            if (direction && direction != d) goto fail;
            direction = d;
        }
        c[k].start = at; c[k].first_glyph = i; c[k++].glyph_count = 1;
    }
    for (i = 0; i < k; ++i) {
        size_t at = c[i].start, end = direction < 0 ? (i ? c[i-1].start : n) : (i+1 < k ? c[i+1].start : n);
        c[i].length = end - at;
        while (at < end) {
            uint32_t cp; size_t used;
            if (!mulib_utf8_decode((const unsigned char *)s+at, end-at, &cp, &used)) goto fail;
            at += used; ++c[i].scalar_count;
        }
    }
    *out = c; *count = k; return 0;
fail:
    free(c); return -1;
}
#ifdef __cplusplus
}
#endif
#endif
