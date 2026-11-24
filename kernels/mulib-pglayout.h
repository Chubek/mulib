/** @file mulib-pglayout.h
 * @brief Owned page objects, styled block flow, Unicode wrapping and JSON/YAML.
 * Define MULIB_IMPL_PGLAYOUT and all transitive switches listed in the manual.
 * Text measurement is caller-supplied; a bidi/HarfBuzz adapter is provided.
 */
#ifndef MULIB_PGLAYOUT_H
#define MULIB_PGLAYOUT_H
#include "mulib-stylesheet.h"
#include "mulib-unibreak.h"
#include "mulib-bidi.h"
#include "mulib-fontshape.h"
#include "mulib-json.h"
#include "mulib-yaml.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_pglayout_kind {
    MULIB_PGLAYOUT_LAYOUT, MULIB_PGLAYOUT_TEXTBLOCK, MULIB_PGLAYOUT_TEXT,
    MULIB_PGLAYOUT_IMAGE, MULIB_PGLAYOUT_PATH
} mulib_pglayout_kind_t;
typedef struct mulib_pglayout_item mulib_pglayout_item_t;
/** @brief Per-compute candidate-measurement budget; override before including.
 * Exhaustion fails transactionally rather than allowing unbounded prefix work. */
#ifndef MULIB_PGLAYOUT_MAX_MEASUREMENTS
#define MULIB_PGLAYOUT_MAX_MEASUREMENTS 1000000
#endif
/** @brief Border box in page pixels, excluding external margin. */
typedef struct mulib_pglayout_box { double x, y, width, height; } mulib_pglayout_box_t;
/** @brief Text byte slice (excluding hard newline), top-left position and size. */
typedef struct mulib_pglayout_line { size_t start, length; mulib_pglayout_box_t box; } mulib_pglayout_line_t;
/** @brief Measure one UTF-8 line in pixels, respecting style. Return 0/-1;
 * callback must return finite nonnegative advance and must not mutate the tree. */
typedef int (*mulib_pglayout_measure_fn)(void *context, const char *text, size_t length,
                                        const mulib_stylesheet_style_t *style, double *advance);
/** @brief Adapter context: borrowed ICU/font, fixed font scale in 26.6 pixels.
 * Caller selects a font/scale matching style before using this simple adapter.
 * One script per directional run; set script/language or let HarfBuzz guess. */
typedef struct mulib_pglayout_font {
    mulib_uniproc_t *unicode;
    mulib_fontshape_t *font;
    const char *script, *language;
} mulib_pglayout_font_t;
/** @brief Ordered borrowed fonts, all scaled in 26.6 pixels. unicode and fonts
 * must remain alive through shaping (and rendering cached glyphs). base uses
 * bidi's 0/1/-1/-2 convention. NULL language means und. No implicit font I/O. */
typedef struct mulib_pglayout_fontset {
    mulib_uniproc_t *unicode;
    mulib_fontshape_t *const *fonts;
    size_t count;
    const char *language;
    int base;
} mulib_pglayout_fontset_t;
/** @brief One visual run: paragraph-absolute source bytes, font-set index,
 * resolved bidi level/script, and owned glyphs with paragraph-absolute clusters. */
typedef struct mulib_pglayout_shaped_run {
    size_t start, length, font_index;
    unsigned level;
    char script[5];
    mulib_fontshape_run_t glyphs;
} mulib_pglayout_shaped_run_t;
typedef struct mulib_pglayout_shaped_line {
    mulib_pglayout_shaped_run_t *runs;
    size_t count;
    unsigned paragraph_level;
    double advance;
} mulib_pglayout_shaped_line_t;
/** @brief Shape a scalar/grapheme-aligned line range in the context of the whole
 * paragraph. Itemizes bidi, script and whole-grapheme font fallback, and shapes
 * each item. The first font covering all non-control/non-ignorable scalars of a
 * grapheme wins. Missing coverage/glyphs fail rather than silently using .notdef.
 * Common/Inherited clusters adopt adjacent strong script (previous preferred).
 * Fonts must match the intended size. 0/-1; output must be empty, transactional. */
int mulib_pglayout_shape_line(const mulib_pglayout_fontset_t *fonts,
                              const char *paragraph, size_t paragraph_length,
                              size_t start, size_t length, mulib_pglayout_shaped_line_t *line);
/** @brief Free shaped runs/glyph arrays and zero the result. */
void mulib_pglayout_shaped_line_destroy(mulib_pglayout_shaped_line_t *line);
/** @brief Bidi-aware HarfBuzz measurement adapter (style is not used). */
int mulib_pglayout_font_measure(void *context, const char *text, size_t length,
                               const mulib_stylesheet_style_t *style, double *advance);
/** @brief Create an empty owned item. Type names are layout,textblock,text,image,path. */
mulib_pglayout_item_t *mulib_pglayout_new(mulib_pglayout_kind_t kind);
/** @brief Destroy a detached/root item and descendants. Attached items are
 * owned by their parent and must not be destroyed separately. */
void mulib_pglayout_destroy(mulib_pglayout_item_t *item);
/** @brief Append a detached child to layout/textblock; ownership on success only.
 * Reject cycles/depth>60. 0/-1. Mutations invalidate prior computed geometry. */
int mulib_pglayout_append(mulib_pglayout_item_t *parent, mulib_pglayout_item_t *child);
/** @brief Set copied XML-compatible selector metadata (id,class,style, etc.). 0/-1. */
int mulib_pglayout_attr(mulib_pglayout_item_t *item, const char *name, const char *value);
/** @brief Set text or an image URI. UTF-8, NUL-terminated, copied; no fetching.
 * Other kinds reject content. 0/-1. Empty content is valid. */
int mulib_pglayout_content(mulib_pglayout_item_t *item, const char *content);
/** @brief Set intrinsic image size in pixels; finite nonnegative sizes. 0/-1. */
int mulib_pglayout_image_size(mulib_pglayout_item_t *item, double width, double height);
/** @brief Set copied polyline/polygon coordinates for a path. count counts points,
 * xy has 2*count doubles; finite nonnegative coordinates. closed=0/1. 0/-1. */
int mulib_pglayout_path(mulib_pglayout_item_t *item, const double *xy, size_t count, int closed);
/** @brief Borrow path coordinates; optional count/closed receive metadata.
 * Returns NULL and zero metadata for other kinds or an empty path. */
const double *mulib_pglayout_points(const mulib_pglayout_item_t *item, size_t *count, int *closed);
/** @brief Borrow selector metadata. Do not mutate/destroy returned XML node. */
const mulib_xml_node_t *mulib_pglayout_node(const mulib_pglayout_item_t *item);
/** @brief Return kind, or -1 for NULL. */
int mulib_pglayout_type(const mulib_pglayout_item_t *item);
/** @brief Borrow text/URI, or NULL for NULL item. */
const char *mulib_pglayout_text(const mulib_pglayout_item_t *item);
/** @brief Child count (zero for NULL). */
size_t mulib_pglayout_count(const mulib_pglayout_item_t *item);
/** @brief Borrow child or NULL out of range. */
mulib_pglayout_item_t *mulib_pglayout_child(const mulib_pglayout_item_t *item, size_t index);
/** @brief Compute vertical block flow, CSS box sizes and Unicode wrapped lines.
 * Root must be detached, width finite >0, <=1e6. All lengths in pixels.
 * Styles inherit through containers. ICU is required only for nonempty text.
 * Measure is required for nonempty text; no monospaced approximation is used.
 * Transactional: failure preserves prior geometry. Up to 100000 items, depth60.
 * Long words wrap at grapheme boundaries; hard newlines preserve blank lines.
 * Explicit sizes can overflow content; layout never clips/rasterizes/fetches. */
int mulib_pglayout_compute(mulib_pglayout_item_t *root, const mulib_stylesheet_t *sheet,
                           mulib_uniproc_t *unicode, double width,
                           mulib_pglayout_measure_fn measure, void *context);
/** @brief Compute using paragraph-aware script/fallback shaping and retain owned
 * glyph runs for each final text line. Fonts are borrowed and fixed-size: every
 * visible text style must match font_size (pixels). All fonts must be scaled to
 * font_size*64 by the caller. Use custom compute for application font selection.
 * Same transactional/resource rules as compute; missing coverage fails. */
int mulib_pglayout_compute_shaped(mulib_pglayout_item_t *root, const mulib_stylesheet_t *sheet,
                                  const mulib_pglayout_fontset_t *fonts, double font_size, double width);
/** @brief Borrow cached shaping for a computed text line, or NULL when absent.
 * Clusters/start offsets refer to that text leaf. Invalidated by any mutation or
 * successful recompute; fonts from the supplied font set are not retained. */
const mulib_pglayout_shaped_line_t *mulib_pglayout_shaped_line(const mulib_pglayout_item_t *item, size_t line);
/** @brief Copy computed box/style; 0/-1 (fails after any tree mutation). */
int mulib_pglayout_geometry(const mulib_pglayout_item_t *item, mulib_pglayout_box_t *box,
                            mulib_stylesheet_style_t *style);
/** @brief Borrow computed lines; count=0 and NULL if unavailable. */
const mulib_pglayout_line_t *mulib_pglayout_lines(const mulib_pglayout_item_t *item, size_t *count);
/** @brief Encode source tree as owned JSON DOM (schema version 1). Computed
 * geometry is derived and must be recomputed after loading. NULL on failure. */
mulib_json_t *mulib_pglayout_to_json(const mulib_pglayout_item_t *item);
/** @brief Decode strict version-1 source schema; reject unknown fields/types,
 * malformed UTF-8, excessive depth/items. Returns detached owned tree or NULL. */
mulib_pglayout_item_t *mulib_pglayout_from_json(const mulib_json_t *json);
/** @brief Serialize source tree to malloc-owned JSON (yaml=0) or YAML (yaml=1). */
char *mulib_pglayout_serialize(const mulib_pglayout_item_t *item, int yaml);
/** @brief Parse JSON/YAML and validate source schema; NULL on any failure. */
mulib_pglayout_item_t *mulib_pglayout_deserialize(const char *source, size_t length, int yaml);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PGLAYOUT) && !defined(MULIB_PGLAYOUT_IMPLEMENTED)
#define MULIB_PGLAYOUT_IMPLEMENTED
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_pglayout_item {
    mulib_pglayout_kind_t kind;
    mulib_xml_node_t *node;
    struct mulib_pglayout_item *parent, **children;
    size_t count, capacity;
    char *content;
    double width, height, *points;
    size_t point_count;
    int closed, valid;
    mulib_pglayout_box_t box;
    mulib_stylesheet_style_t style;
    mulib_pglayout_line_t *lines;
    size_t line_count;
    mulib_pglayout_shaped_line_t *shaped;
};
static const char *const mulib_pglayout_names_[] = {"layout", "textblock", "text", "image", "path"};
static void mulib_pglayout_invalidate_(mulib_pglayout_item_t *p) {
    while (p->parent) p = p->parent;
    p->valid = 0;
}
static int mulib_pglayout_valid_(const mulib_pglayout_item_t *p) {
    if (!p) return 0;
    while (p->parent) p = p->parent;
    return p->valid;
}
mulib_pglayout_item_t *mulib_pglayout_new(mulib_pglayout_kind_t kind) {
    mulib_pglayout_item_t *p;
    if (kind < MULIB_PGLAYOUT_LAYOUT || kind > MULIB_PGLAYOUT_PATH) return NULL;
    p = (mulib_pglayout_item_t *)calloc(1, sizeof *p);
    if (!p) return NULL;
    p->kind = kind; p->style = mulib_stylesheet_default();
    p->node = mulib_xml_new(MULIB_XML_ELEMENT, mulib_pglayout_names_[kind]);
    p->content = (char *)calloc(1, 1);
    if (!p->node || !p->content) { mulib_pglayout_destroy(p); return NULL; }
    return p;
}
void mulib_pglayout_destroy(mulib_pglayout_item_t *p) {
    size_t i;
    if (!p || p->parent) return;
    for (i = 0; i < p->count; ++i) { p->children[i]->parent = NULL; mulib_pglayout_destroy(p->children[i]); }
    for (i = 0; i < p->line_count; ++i) if (p->shaped) mulib_pglayout_shaped_line_destroy(&p->shaped[i]);
    mulib_xml_destroy(p->node); free(p->children); free(p->content); free(p->points); free(p->lines); free(p->shaped); free(p);
}
static size_t mulib_pglayout_depth_(const mulib_pglayout_item_t *p) {
    size_t i, depth = 1;
    for (i = 0; i < p->count; ++i) {
        size_t d = 1 + mulib_pglayout_depth_(p->children[i]);
        if (d > depth) depth = d;
    }
    return depth;
}
int mulib_pglayout_append(mulib_pglayout_item_t *p, mulib_pglayout_item_t *c) {
    mulib_pglayout_item_t *a, **next;
    size_t depth = 0, capacity;
    if (!p || !c || c->parent || p->kind > MULIB_PGLAYOUT_TEXTBLOCK || p->count >= 100000) return -1;
    for (a = p; a; a = a->parent) { if (a == c) return -1; ++depth; }
    if (depth + mulib_pglayout_depth_(c) > 60 || p->count >= SIZE_MAX / sizeof *next) return -1;
    if (p->count == p->capacity) {
        capacity = p->capacity ? p->capacity*2 : 8;
        if (capacity > 100000) capacity = 100000;
        if (capacity > SIZE_MAX / sizeof *next) return -1;
        next = (mulib_pglayout_item_t **)realloc(p->children, capacity * sizeof *next);
        if (!next) return -1;
        p->children = next; p->capacity = capacity;
    }
    if (!mulib_xml_append(p->node, c->node)) return -1;
    p->children[p->count++] = c; c->parent = p; mulib_pglayout_invalidate_(p); return 0;
}
int mulib_pglayout_attr(mulib_pglayout_item_t *p, const char *name, const char *value) {
    if (!p || !mulib_xml_set_attr(p->node, name, value)) return -1;
    mulib_pglayout_invalidate_(p); return 0;
}
int mulib_pglayout_content(mulib_pglayout_item_t *p, const char *s) {
    size_t n;
    char *copy;
    if (!p || !s || (p->kind != MULIB_PGLAYOUT_TEXT && p->kind != MULIB_PGLAYOUT_IMAGE)) return -1;
    n = strlen(s);
    if (n > INT32_MAX || !mulib_utf8_validate((const unsigned char *)s, n)) return -1;
    copy = (char *)malloc(n + 1);
    if (!copy) return -1;
    memcpy(copy, s, n + 1); free(p->content); p->content = copy; mulib_pglayout_invalidate_(p); return 0;
}
int mulib_pglayout_image_size(mulib_pglayout_item_t *p, double w, double h) {
    if (!p || p->kind != MULIB_PGLAYOUT_IMAGE || !isfinite(w) || !isfinite(h) || w < 0 || h < 0 || w > 1e6 || h > 1e6) return -1;
    p->width = w; p->height = h; mulib_pglayout_invalidate_(p); return 0;
}
int mulib_pglayout_path(mulib_pglayout_item_t *p, const double *xy, size_t n, int closed) {
    double *copy = NULL, w = 0, h = 0;
    size_t i;
    if (!p || p->kind != MULIB_PGLAYOUT_PATH || (!xy && n) || n > SIZE_MAX / sizeof *copy / 2 || (closed != 0 && closed != 1)) return -1;
    for (i = 0; i < n; ++i) {
        if (!isfinite(xy[2*i]) || !isfinite(xy[2*i+1]) || xy[2*i] < 0 || xy[2*i+1] < 0 || xy[2*i] > 1e6 || xy[2*i+1] > 1e6) return -1;
        if (xy[2*i] > w) w = xy[2*i];
        if (xy[2*i+1] > h) h = xy[2*i+1];
    }
    if (n) { copy = (double *)malloc(2*n*sizeof *copy); if (!copy) return -1; memcpy(copy, xy, 2*n*sizeof *copy); }
    free(p->points); p->points = copy; p->point_count = n; p->closed = closed;
    p->width = w; p->height = h; mulib_pglayout_invalidate_(p); return 0;
}
const mulib_xml_node_t *mulib_pglayout_node(const mulib_pglayout_item_t *p) { return p ? p->node : NULL; }
const double *mulib_pglayout_points(const mulib_pglayout_item_t *p, size_t *n, int *closed) {
    int valid = p && p->kind == MULIB_PGLAYOUT_PATH;
    if (n) *n = valid ? p->point_count : 0;
    if (closed) *closed = valid && p->point_count ? p->closed : 0;
    return valid ? p->points : NULL;
}
int mulib_pglayout_type(const mulib_pglayout_item_t *p) { return p ? (int)p->kind : -1; }
const char *mulib_pglayout_text(const mulib_pglayout_item_t *p) { return p ? p->content : NULL; }
size_t mulib_pglayout_count(const mulib_pglayout_item_t *p) { return p ? p->count : 0; }
mulib_pglayout_item_t *mulib_pglayout_child(const mulib_pglayout_item_t *p, size_t i) { return p && i < p->count ? p->children[i] : NULL; }
int mulib_pglayout_geometry(const mulib_pglayout_item_t *p, mulib_pglayout_box_t *b, mulib_stylesheet_style_t *s) {
    if (!mulib_pglayout_valid_(p)) return -1;
    if (b) *b = p->box;
    if (s) *s = p->style;
    return 0;
}
const mulib_pglayout_line_t *mulib_pglayout_lines(const mulib_pglayout_item_t *p, size_t *n) {
    if (n) *n = mulib_pglayout_valid_(p) ? p->line_count : 0;
    return mulib_pglayout_valid_(p) ? p->lines : NULL;
}
const mulib_pglayout_shaped_line_t *mulib_pglayout_shaped_line(const mulib_pglayout_item_t *p, size_t i) {
    return mulib_pglayout_valid_(p) && p->shaped && i < p->line_count ? &p->shaped[i] : NULL;
}
void mulib_pglayout_shaped_line_destroy(mulib_pglayout_shaped_line_t *line) {
    size_t i;
    if (!line) return;
    for (i = 0; i < line->count; ++i) mulib_fontshape_run_destroy(&line->runs[i].glyphs);
    free(line->runs); memset(line, 0, sizeof *line);
}
typedef struct mulib_pglayout_cluster_ {
    size_t start, end, font;
    char script[5];
} mulib_pglayout_cluster_;
static int mulib_pglayout_fontset_valid_(const mulib_pglayout_fontset_t *f) {
    size_t i;
    if (!f || !f->unicode || !f->fonts || !f->count || f->count > SIZE_MAX / sizeof *f->fonts ||
        f->base < -2 || f->base > 1) return 0;
    for (i = 0; i < f->count; ++i) if (!f->fonts[i]) return 0;
    return 1;
}
static int mulib_pglayout_shape_(const mulib_pglayout_fontset_t *fonts, const char *s, size_t bytes,
                                mulib_bidi_paragraph_t *paragraph, size_t start, size_t length,
                                mulib_pglayout_shaped_line_t *out,
                                const mulib_unibreak_boundary_t *prepared, size_t prepared_count) {
    mulib_pglayout_shaped_line_t line = {0};
    mulib_bidi_result_t bidi = {0};
    mulib_unibreak_boundary_t *owned_boundaries = NULL;
    const mulib_unibreak_boundary_t *boundaries = prepared;
    mulib_pglayout_cluster_ *clusters = NULL;
    size_t count = prepared_count, n = 0, i, j, k, ri, capacity = 0;
    int result = -1;
    if (!out || out->runs || out->count || start > bytes || length > bytes-start ||
        mulib_bidi_paragraph_line(paragraph, start, length, &bidi)) return -1;
    line.paragraph_level = bidi.paragraph_level;
    if (!length) { *out = line; mulib_bidi_destroy(&bidi); return 0; }
    /* Whole-text boundaries retain preceding context (RI pairs/emoji sequences). */
    if (!boundaries) {
        if (mulib_unibreak_find(fonts->unicode, MULIB_UNIBREAK_GRAPHEME, NULL, s, bytes, &owned_boundaries, &count)) goto done;
        boundaries = owned_boundaries;
    }
    for (i = 0; i < count && boundaries[i].offset < start; ++i) {}
    if (i == count || boundaries[i].offset != start) goto done;
    for (j = i; j < count && boundaries[j].offset < start+length; ++j) {}
    if (j == count || boundaries[j].offset != start+length) goto done;
    n = j-i;
    if (n > SIZE_MAX / sizeof *clusters || n > SIZE_MAX / sizeof *line.runs) goto done;
    clusters = (mulib_pglayout_cluster_ *)calloc(n, sizeof *clusters);
    line.runs = (mulib_pglayout_shaped_run_t *)calloc(n, sizeof *line.runs);
    if (!clusters || !line.runs) goto done;
    capacity = n;
    for (k = 0; k < n; ++k) {
        size_t at = boundaries[i+k].offset, end = boundaries[i+k+1].offset, used;
        uint32_t cp;
        char script[5] = "Zyyy";
        clusters[k].start = at; clusters[k].end = end;
        while (at < end) {
            char tag[5];
            if (!mulib_utf8_decode((const unsigned char *)s+at, end-at, &cp, &used) ||
                mulib_uniproc_script(fonts->unicode, cp, tag, NULL)) goto done;
            if (strcmp(tag, "Zyyy") && strcmp(tag, "Zinh")) memcpy(script, tag, 5);
            at += used;
        }
        memcpy(clusters[k].script, script, 5);
        for (j = 0; j < fonts->count; ++j) {
            int covers = 1;
            at = clusters[k].start;
            while (at < end) {
                int category, ignorable;
                if (!mulib_utf8_decode((const unsigned char *)s+at, end-at, &cp, &used) ||
                    mulib_uniproc_properties(fonts->unicode, cp, &category, NULL, NULL, NULL) ||
                    mulib_uniproc_script(fonts->unicode, cp, NULL, &ignorable)) goto done;
                if (!ignorable && !(category >= 13 && category <= 16) &&
                    mulib_fontshape_covers(fonts->fonts[j], cp) != 1) { covers = 0; break; }
                at += used;
            }
            if (covers) break;
        }
        if (j == fonts->count) goto done;
        clusters[k].font = j;
    }
    /* Resolve neutral/inherited clusters without mixing strong-script items. */
    for (k = 0; k < n; ++k) if (!strcmp(clusters[k].script, "Zyyy") || !strcmp(clusters[k].script, "Zinh")) {
        if (k && strcmp(clusters[k-1].script, "Zyyy")) memcpy(clusters[k].script, clusters[k-1].script, 5);
        else for (j = k+1; j < n; ++j) if (strcmp(clusters[j].script, "Zyyy") && strcmp(clusters[j].script, "Zinh")) {
            memcpy(clusters[k].script, clusters[j].script, 5); break;
        }
    }
    for (ri = 0; ri < bidi.count; ++ri) {
        size_t first = line.count, begin = bidi.runs[ri].start, end = begin+bidi.runs[ri].length;
        for (k = 0; k < n && clusters[k].end <= begin; ++k) {}
        /* Directional boundaries must never force a split inside a grapheme. */
        if (k == n || clusters[k].start != begin) goto done;
        while (k < n && clusters[k].start < end) {
            mulib_pglayout_shaped_run_t *run;
            mulib_fontshape_options_t options = {0};
            size_t next = k+1;
            while (next < n && clusters[next].start < end && clusters[next].font == clusters[k].font &&
                   !strcmp(clusters[next].script, clusters[k].script)) ++next;
            if (clusters[next-1].end > end || line.count == capacity) goto done;
            run = &line.runs[line.count++];
            run->start = clusters[k].start; run->length = clusters[next-1].end-run->start;
            run->font_index = clusters[k].font; run->level = bidi.runs[ri].level;
            memcpy(run->script, clusters[k].script, 5);
            options.direction = run->level & 1 ? "rtl" : "ltr";
            options.script = run->script; options.language = fonts->language;
            if (mulib_fontshape_shape(fonts->fonts[run->font_index], s+run->start, run->length, &options, &run->glyphs)) goto done;
            for (j = 0; j < run->glyphs.count; ++j) {
                if (!run->glyphs.glyphs[j].glyph) goto done;
                run->glyphs.glyphs[j].cluster += (uint32_t)run->start;
                line.advance += run->glyphs.glyphs[j].x_advance/64.0;
            }
            k = next;
        }
        if (k && clusters[k-1].end != end) goto done;
        if (bidi.runs[ri].level & 1) {
            size_t left = first, right = line.count;
            while (left < right && left < --right) {
                mulib_pglayout_shaped_run_t tmp = line.runs[left];
                line.runs[left++] = line.runs[right]; line.runs[right] = tmp;
            }
        }
    }
    if (!isfinite(line.advance) || line.advance < 0) goto done;
    *out = line; memset(&line, 0, sizeof line); result = 0;
done:
    mulib_pglayout_shaped_line_destroy(&line); mulib_bidi_destroy(&bidi);
    free(owned_boundaries); free(clusters); return result;
}
int mulib_pglayout_shape_line(const mulib_pglayout_fontset_t *fonts, const char *s, size_t bytes,
                              size_t start, size_t length, mulib_pglayout_shaped_line_t *out) {
    mulib_bidi_paragraph_t *paragraph;
    int result;
    if (!mulib_pglayout_fontset_valid_(fonts) || !out || out->runs || out->count) return -1;
    paragraph = mulib_bidi_paragraph_open(fonts->unicode, s, bytes, fonts->base);
    if (!paragraph) return -1;
    result = mulib_pglayout_shape_(fonts, s, bytes, paragraph, start, length, out, NULL, 0);
    mulib_bidi_paragraph_close(paragraph); return result;
}
int mulib_pglayout_font_measure(void *context, const char *s, size_t n, const mulib_stylesheet_style_t *style, double *advance) {
    mulib_pglayout_font_t *f = (mulib_pglayout_font_t *)context;
    mulib_bidi_result_t bidi = {0};
    double total = 0;
    size_t i, j;
    (void)style;
    if (!f || !f->font || !advance || mulib_bidi_resolve(f->unicode, s, n, -1, &bidi)) return -1;
    for (i = 0; i < bidi.count; ++i) {
        mulib_fontshape_run_t run = {0};
        mulib_fontshape_options_t options = {bidi.runs[i].level & 1 ? "rtl" : "ltr", f->script, f->language, NULL, 0};
        if (mulib_fontshape_shape(f->font, s + bidi.runs[i].start, bidi.runs[i].length, &options, &run)) {
            mulib_bidi_destroy(&bidi); return -1;
        }
        for (j = 0; j < run.count; ++j) total += run.glyphs[j].x_advance / 64.0;
        mulib_fontshape_run_destroy(&run);
    }
    mulib_bidi_destroy(&bidi);
    if (!isfinite(total) || total < 0) return -1;
    *advance = total; return 0;
}
typedef struct mulib_pglayout_work_ {
    mulib_pglayout_item_t *item;
    mulib_pglayout_box_t box;
    mulib_stylesheet_style_t style;
    mulib_pglayout_line_t *lines;
    size_t count;
    mulib_pglayout_shaped_line_t *shaped;
} mulib_pglayout_work_;
typedef struct mulib_pglayout_shaper_ {
    const mulib_pglayout_fontset_t *fonts;
    const char *text;
    size_t length;
    mulib_bidi_paragraph_t *paragraph;
    const mulib_unibreak_boundary_t *boundaries;
    size_t count;
} mulib_pglayout_shaper_;
static int mulib_pglayout_shaper_measure_(void *context, const char *text, size_t length,
                                         const mulib_stylesheet_style_t *style, double *advance) {
    mulib_pglayout_shaper_ *s = (mulib_pglayout_shaper_ *)context;
    mulib_pglayout_shaped_line_t line = {0};
    int result;
    (void)style;
    result = mulib_pglayout_shape_(s->fonts, s->text, s->length, s->paragraph,
                                  (size_t)(text-s->text), length, &line, s->boundaries, s->count);
    if (!result) *advance = line.advance;
    mulib_pglayout_shaped_line_destroy(&line); return result;
}
static size_t mulib_pglayout_trim_(const char *s, size_t start, size_t end) {
    if (end > start && s[end-1] == '\n') { --end; if (end > start && s[end-1] == '\r') --end; }
    else if (end > start && (s[end-1] == '\r' || s[end-1] == '\v' || s[end-1] == '\f')) --end;
    else if (end-start >= 2 && (unsigned char)s[end-2] == 0xc2 && (unsigned char)s[end-1] == 0x85) end -= 2;
    else if (end-start >= 3 && (unsigned char)s[end-3] == 0xe2 && (unsigned char)s[end-2] == 0x80 &&
             ((unsigned char)s[end-1] == 0xa8 || (unsigned char)s[end-1] == 0xa9)) end -= 3;
    return end;
}
static int mulib_pglayout_measure_(mulib_pglayout_measure_fn fn, void *ctx, const char *s, size_t start, size_t end,
                                   const mulib_stylesheet_style_t *style, double *width, size_t *measurements) {
    if (*measurements >= MULIB_PGLAYOUT_MAX_MEASUREMENTS) return -1;
    ++*measurements;
    end = mulib_pglayout_trim_(s, start, end);
    if (end == start) { *width = 0; return 0; }
    *width = NAN;
    if (!fn || fn(ctx, s + start, end-start, style, width) || !isfinite(*width) || *width < 0) return -1;
    return 0;
}
static int mulib_pglayout_wrap_(mulib_pglayout_work_ *w, mulib_uniproc_t *u, double width,
                               mulib_pglayout_measure_fn fn, void *ctx, size_t *measurements,
                               const mulib_pglayout_fontset_t *fonts) {
    const char *s = w->item->content;
    size_t n = strlen(s), nb = 0, ng = 0, start = 0, bi = 1, gi = 1, i;
    mulib_unibreak_boundary_t *breaks = NULL, *graphemes = NULL;
    double line_height = w->style.font_size * w->style.line_height;
    int status = -1;
    mulib_pglayout_shaper_ shaper = {fonts, s, n, NULL, NULL, 0};
    if (fonts) {
        shaper.paragraph = mulib_bidi_paragraph_open(fonts->unicode, s, n, fonts->base);
        if (!shaper.paragraph) goto done;
        fn = mulib_pglayout_shaper_measure_; ctx = &shaper;
    }
    if (n && (mulib_unibreak_find(u, MULIB_UNIBREAK_LINE, NULL, s, n, &breaks, &nb) ||
              mulib_unibreak_find(u, MULIB_UNIBREAK_GRAPHEME, NULL, s, n, &graphemes, &ng))) goto done;
    shaper.boundaries = graphemes; shaper.count = ng;
    if (ng > SIZE_MAX / sizeof *w->lines - 1) goto done;
    w->lines = (mulib_pglayout_line_t *)calloc(ng + 1, sizeof *w->lines);
    if (!w->lines) goto done;
    do {
        size_t end = start, hard = n, trim;
        double measured = 0;
        while (bi < nb && breaks[bi].offset <= start) ++bi;
        for (i = bi; i < nb; ++i) {
            double candidate;
            if (breaks[i].status >= 100) hard = breaks[i].offset;
            if (mulib_pglayout_measure_(fn, ctx, s, start, breaks[i].offset, &w->style, &candidate, measurements)) goto done;
            if (candidate > width) break;
            end = breaks[i].offset; measured = candidate;
            if (breaks[i].status >= 100) break;
        }
        if (end == start && start < n) {
            size_t limit = bi < nb ? breaks[bi].offset : hard;
            while (gi < ng && graphemes[gi].offset <= start) ++gi;
            for (i = gi; i < ng && graphemes[i].offset <= limit; ++i) {
                double candidate;
                if (mulib_pglayout_measure_(fn, ctx, s, start, graphemes[i].offset, &w->style, &candidate, measurements)) goto done;
                if (end > start && candidate > width) break;
                end = graphemes[i].offset; measured = candidate;
                if (candidate > width) break;
            }
            if (end == start) goto done;
        }
        trim = mulib_pglayout_trim_(s, start, end);
        w->lines[w->count].start = start; w->lines[w->count].length = trim-start;
        w->lines[w->count].box.x = w->box.x + w->style.padding +
            (w->style.align == 1 ? (width-measured)/2 : w->style.align == 2 ? width-measured : 0);
        w->lines[w->count].box.y = w->box.y + w->style.padding + w->count * line_height;
        w->lines[w->count].box.width = measured; w->lines[w->count].box.height = line_height;
        if (!isfinite(w->lines[w->count].box.x) || !isfinite(w->lines[w->count].box.y)) goto done;
        ++w->count; start = end;
        if (end == n) {
            if (trim < end) {
                w->lines[w->count].start = n;
                w->lines[w->count].box.x = w->box.x + w->style.padding +
                    (w->style.align == 1 ? width/2 : w->style.align == 2 ? width : 0);
                w->lines[w->count].box.y = w->box.y + w->style.padding + w->count * line_height;
                w->lines[w->count++].box.height = line_height;
            }
            break;
        }
    } while (start < n);
    w->box.height = w->count * line_height + 2*w->style.padding;
    if (fonts) {
        size_t j;
        if (w->count > SIZE_MAX / sizeof *w->shaped) goto done;
        w->shaped = (mulib_pglayout_shaped_line_t *)calloc(w->count, sizeof *w->shaped);
        if (!w->shaped) goto done;
        for (j = 0; j < w->count; ++j)
            if (mulib_pglayout_shape_(fonts, s, n, shaper.paragraph, w->lines[j].start,
                                      w->lines[j].length, &w->shaped[j], graphemes, ng)) goto done;
    }
    status = 0;
done:
    mulib_bidi_paragraph_close(shaper.paragraph);
    free(breaks); free(graphemes); return status;
}
static int mulib_pglayout_total_(const mulib_pglayout_item_t *p, size_t *total) {
    size_t i;
    if (++*total > 100000) return -1;
    for (i = 0; i < p->count; ++i) if (mulib_pglayout_total_(p->children[i], total)) return -1;
    return 0;
}
static int mulib_pglayout_flow_(mulib_pglayout_item_t *p, mulib_pglayout_work_ *work, size_t *used,
                               const mulib_stylesheet_t *sheet, const mulib_stylesheet_style_t *parent,
                               mulib_uniproc_t *u, double x, double y, double available, int hidden,
                               mulib_pglayout_measure_fn fn, void *ctx, double *extent, size_t *measurements,
                               const mulib_pglayout_fontset_t *fonts, double font_size) {
    mulib_pglayout_work_ *w = &work[(*used)++];
    size_t i;
    double width, height = 0;
    w->item = p;
    if (mulib_stylesheet_resolve(sheet, p->node, parent, &w->style, NULL)) return -1;
    hidden = hidden || w->style.hidden;
    width = w->style.width >= 0 ? w->style.width : fmax(0, available - 2*(w->style.margin+w->style.padding));
    if (p->kind >= MULIB_PGLAYOUT_IMAGE && w->style.width < 0) width = p->width;
    w->box.x = x + w->style.margin; w->box.y = y + w->style.margin;
    w->box.width = width + 2*w->style.padding;
    if (p->kind == MULIB_PGLAYOUT_TEXT && !hidden) {
        if ((fonts && w->style.font_size != font_size) ||
            mulib_pglayout_wrap_(w, u, width, fn, ctx, measurements, fonts)) return -1;
        height = w->box.height - 2*w->style.padding;
    } else if (p->kind >= MULIB_PGLAYOUT_IMAGE) {
        height = p->height;
        if (p->kind == MULIB_PGLAYOUT_IMAGE && p->width > 0 && w->style.width >= 0 && w->style.height < 0) {
            /* Compute product before quotient in extended precision: dividing
             * first can overflow even when the final ratio is representable. */
            long double scaled = (long double)p->height * width / p->width;
            if (!isfinite(scaled) || scaled > DBL_MAX) return -1;
            height = (double)scaled;
        }
    }
    for (i = 0; i < p->count; ++i) {
        double child_height;
        if (mulib_pglayout_flow_(p->children[i], work, used, sheet, &w->style, u,
                                w->box.x+w->style.padding, w->box.y+w->style.padding+height,
                                width, hidden, fn, ctx, &child_height, measurements, fonts, font_size)) return -1;
        height += child_height;
    }
    if (w->style.height >= 0) height = w->style.height;
    w->box.height = height + 2*w->style.padding;
    if (hidden) { memset(&w->box, 0, sizeof w->box); *extent = 0; }
    else *extent = w->box.height + 2*w->style.margin;
    return isfinite(*extent) && isfinite(w->box.x) && isfinite(w->box.y) && isfinite(w->box.width) ? 0 : -1;
}
static int mulib_pglayout_compute_(mulib_pglayout_item_t *root, const mulib_stylesheet_t *sheet, mulib_uniproc_t *u,
                                  double width, mulib_pglayout_measure_fn fn, void *ctx,
                                  const mulib_pglayout_fontset_t *fonts, double font_size) {
    mulib_pglayout_work_ *work;
    size_t count = 0, used = 0, i, measurements = 0;
    double extent;
    int status;
    if (!root || root->parent || !isfinite(width) || width <= 0 || width > 1e6 ||
        mulib_pglayout_total_(root, &count) || count > SIZE_MAX / sizeof *work) return -1;
    work = (mulib_pglayout_work_ *)calloc(count, sizeof *work);
    if (!work) return -1;
    status = mulib_pglayout_flow_(root, work, &used, sheet, NULL, u, 0, 0, width, 0, fn, ctx, &extent, &measurements, fonts, font_size);
    if (!status) for (i = 0; i < count; ++i) {
        mulib_pglayout_item_t *p = work[i].item;
        size_t j;
        for (j = 0; j < p->line_count; ++j) if (p->shaped) mulib_pglayout_shaped_line_destroy(&p->shaped[j]);
        free(p->shaped); p->shaped = work[i].shaped; work[i].shaped = NULL;
        free(p->lines); p->lines = work[i].lines; work[i].lines = NULL; p->line_count = work[i].count;
        p->box = work[i].box; p->style = work[i].style;
    }
    for (i = 0; i < count; ++i) {
        size_t j;
        for (j = 0; j < work[i].count; ++j) if (work[i].shaped) mulib_pglayout_shaped_line_destroy(&work[i].shaped[j]);
        free(work[i].shaped); free(work[i].lines);
    }
    free(work);
    if (!status) root->valid = 1;
    return status;
}
int mulib_pglayout_compute(mulib_pglayout_item_t *root, const mulib_stylesheet_t *sheet, mulib_uniproc_t *u,
                           double width, mulib_pglayout_measure_fn fn, void *ctx) {
    return mulib_pglayout_compute_(root, sheet, u, width, fn, ctx, NULL, 0);
}
int mulib_pglayout_compute_shaped(mulib_pglayout_item_t *root, const mulib_stylesheet_t *sheet,
                                  const mulib_pglayout_fontset_t *fonts, double size, double width) {
    if (!mulib_pglayout_fontset_valid_(fonts) || !isfinite(size) || size <= 0 || size > 1e6) return -1;
    return mulib_pglayout_compute_(root, sheet, fonts->unicode, width, NULL, NULL, fonts, size);
}
static int mulib_pglayout_set_(mulib_json_t *o, const char *key, mulib_json_t *v) {
    if (!v) return -1;
    if (!mulib_json_set(o, key, v)) { mulib_json_destroy(v); return -1; }
    return 0;
}
static mulib_json_t *mulib_pglayout_encode_(const mulib_pglayout_item_t *p) {
    mulib_json_t *o = mulib_json_new(MULIB_JSON_OBJECT), *a = NULL;
    size_t i;
    if (!o || mulib_pglayout_set_(o, "type", mulib_json_string(mulib_pglayout_names_[p->kind]))) goto fail;
    a = mulib_json_new(MULIB_JSON_OBJECT);
    if (!a) goto fail;
    for (i = 0; i < mulib_xml_attr_count(p->node); ++i)
        if (mulib_pglayout_set_(a, mulib_xml_attr_name(p->node, i), mulib_json_string(mulib_xml_attr_value(p->node, i)))) goto fail;
    if (!mulib_json_set(o, "attrs", a)) goto fail;
    a = NULL;
    if (p->kind == MULIB_PGLAYOUT_TEXT || p->kind == MULIB_PGLAYOUT_IMAGE)
        if (mulib_pglayout_set_(o, "content", mulib_json_string(p->content))) goto fail;
    if (p->kind == MULIB_PGLAYOUT_IMAGE) {
        if (mulib_pglayout_set_(o, "width", mulib_json_number(p->width)) ||
            mulib_pglayout_set_(o, "height", mulib_json_number(p->height))) goto fail;
    }
    if (p->kind == MULIB_PGLAYOUT_PATH) {
        if (mulib_pglayout_set_(o, "closed", mulib_json_bool(p->closed))) goto fail;
        a = mulib_json_new(MULIB_JSON_ARRAY);
        if (!a) goto fail;
        for (i = 0; i < 2*p->point_count; ++i) {
            mulib_json_t *v = mulib_json_number(p->points[i]);
            if (!v || !mulib_json_append(a, v)) { mulib_json_destroy(v); goto fail; }
        }
        if (!mulib_json_set(o, "points", a)) goto fail;
        a = NULL;
    }
    if (p->kind <= MULIB_PGLAYOUT_TEXTBLOCK) {
        a = mulib_json_new(MULIB_JSON_ARRAY);
        if (!a) goto fail;
        for (i = 0; i < p->count; ++i) {
            mulib_json_t *v = mulib_pglayout_encode_(p->children[i]);
            if (!v || !mulib_json_append(a, v)) { mulib_json_destroy(v); goto fail; }
        }
        if (!mulib_json_set(o, "children", a)) goto fail;
        a = NULL;
    }
    return o;
fail:
    mulib_json_destroy(a); mulib_json_destroy(o); return NULL;
}
mulib_json_t *mulib_pglayout_to_json(const mulib_pglayout_item_t *p) {
    mulib_json_t *o;
    size_t count = 0;
    if (!p || mulib_pglayout_total_(p, &count)) return NULL;
    o = mulib_json_new(MULIB_JSON_OBJECT);
    if (!o) return NULL;
    if (mulib_pglayout_set_(o, "version", mulib_json_number(1)) ||
        mulib_pglayout_set_(o, "root", mulib_pglayout_encode_(p))) { mulib_json_destroy(o); return NULL; }
    return o;
}
static const char *mulib_pglayout_string_(const mulib_json_t *v) {
    size_t n;
    const char *s = mulib_json_text(v, &n);
    return s && strlen(s) == n ? s : NULL;
}
static mulib_pglayout_item_t *mulib_pglayout_decode_(const mulib_json_t *o, size_t depth, size_t *total) {
    const char *type;
    int kind;
    size_t i, expected, n;
    mulib_pglayout_item_t *p = NULL;
    const mulib_json_t *a;
    double *points = NULL;
    if (depth > 60 || ++*total > 100000 || mulib_json_typeof(o) != MULIB_JSON_OBJECT) return NULL;
    type = mulib_pglayout_string_(mulib_json_get(o, "type"));
    if (!type) return NULL;
    for (kind = 0; kind < 5; ++kind) if (!strcmp(type, mulib_pglayout_names_[kind])) break;
    if (kind == 5) return NULL;
    expected = kind <= MULIB_PGLAYOUT_TEXTBLOCK ? 3 : kind == MULIB_PGLAYOUT_TEXT ? 3 : kind == MULIB_PGLAYOUT_IMAGE ? 5 : 4;
    if (mulib_json_size(o) != expected) return NULL;
    p = mulib_pglayout_new((mulib_pglayout_kind_t)kind);
    if (!p) return NULL;
    a = mulib_json_get(o, "attrs");
    if (mulib_json_typeof(a) != MULIB_JSON_OBJECT) goto fail;
    for (i = 0; i < mulib_json_size(a); ++i) {
        const char *key = mulib_json_key(a, i, &n), *value = mulib_pglayout_string_(mulib_json_at(a, i));
        if (!key || strlen(key) != n || !value || mulib_pglayout_attr(p, key, value)) goto fail;
    }
    if (kind == MULIB_PGLAYOUT_TEXT || kind == MULIB_PGLAYOUT_IMAGE) {
        const char *s = mulib_pglayout_string_(mulib_json_get(o, "content"));
        if (!s || mulib_pglayout_content(p, s)) goto fail;
    }
    if (kind == MULIB_PGLAYOUT_IMAGE) {
        const mulib_json_t *w = mulib_json_get(o, "width"), *h = mulib_json_get(o, "height");
        if (mulib_json_typeof(w) != MULIB_JSON_NUMBER || mulib_json_typeof(h) != MULIB_JSON_NUMBER ||
            mulib_pglayout_image_size(p, mulib_json_numeric(w), mulib_json_numeric(h))) goto fail;
    }
    if (kind == MULIB_PGLAYOUT_PATH) {
        const mulib_json_t *closed = mulib_json_get(o, "closed");
        a = mulib_json_get(o, "points"); n = mulib_json_size(a);
        if (mulib_json_typeof(a) != MULIB_JSON_ARRAY || mulib_json_typeof(closed) != MULIB_JSON_BOOL ||
            n % 2 || n > SIZE_MAX / sizeof *points) goto fail;
        if (n) { points = (double *)malloc(n * sizeof *points); if (!points) goto fail; }
        for (i = 0; i < n; ++i) {
            if (mulib_json_typeof(mulib_json_at(a, i)) != MULIB_JSON_NUMBER) goto fail;
            points[i] = mulib_json_numeric(mulib_json_at(a, i));
        }
        if (mulib_pglayout_path(p, points, n/2, mulib_json_boolean(closed))) goto fail;
        free(points); points = NULL;
    }
    if (kind <= MULIB_PGLAYOUT_TEXTBLOCK) {
        a = mulib_json_get(o, "children");
        if (mulib_json_typeof(a) != MULIB_JSON_ARRAY) goto fail;
        for (i = 0; i < mulib_json_size(a); ++i) {
            mulib_pglayout_item_t *child = mulib_pglayout_decode_(mulib_json_at(a, i), depth+1, total);
            if (!child || mulib_pglayout_append(p, child)) { mulib_pglayout_destroy(child); goto fail; }
        }
    }
    return p;
fail:
    free(points); mulib_pglayout_destroy(p); return NULL;
}
mulib_pglayout_item_t *mulib_pglayout_from_json(const mulib_json_t *o) {
    const mulib_json_t *v = mulib_json_get(o, "version");
    size_t total = 0;
    if (mulib_json_typeof(o) != MULIB_JSON_OBJECT || mulib_json_size(o) != 2 ||
        mulib_json_typeof(v) != MULIB_JSON_NUMBER || mulib_json_numeric(v) != 1) return NULL;
    return mulib_pglayout_decode_(mulib_json_get(o, "root"), 1, &total);
}
char *mulib_pglayout_serialize(const mulib_pglayout_item_t *p, int yaml) {
    mulib_json_t *o;
    char *s;
    if (yaml != 0 && yaml != 1) return NULL;
    o = mulib_pglayout_to_json(p);
    if (!o) return NULL;
    s = yaml ? mulib_yaml_stringify(o, NULL) : mulib_json_stringify(o, 1);
    mulib_json_destroy(o); return s;
}
mulib_pglayout_item_t *mulib_pglayout_deserialize(const char *s, size_t n, int yaml) {
    mulib_json_t *o;
    mulib_pglayout_item_t *p;
    if (yaml != 0 && yaml != 1) return NULL;
    o = yaml ? mulib_yaml_parse(s, n, NULL) : mulib_json_parse(s, n, NULL);
    if (!o) return NULL;
    p = mulib_pglayout_from_json(o); mulib_json_destroy(o); return p;
}
#ifdef __cplusplus
}
#endif
#endif
