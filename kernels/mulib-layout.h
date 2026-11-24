/** @file mulib-layout.h
 * @brief Unicode paragraph layout and weighted rectangles, backend-neutral.
 * Define MULIB_IMPL_LAYOUT and transitive switches listed in the manual.
 */
#ifndef MULIB_LAYOUT_H
#define MULIB_LAYOUT_H
#include <stddef.h>
#include "mulib-glyph.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_layout_rect
  {
    double x, y, width, height;
  } mulib_layout_rect_t;
  enum
  {
    MULIB_LAYOUT_ROW = 0,
    MULIB_LAYOUT_COLUMN = 1
  };
  /** @brief Divide parent's main axis by nonnegative weights, inserting gap.
   * All inputs finite, nonnegative sizes/gap, positive sum for nonempty input.
   * 1 success, 0 invalid/overflow/OOM. Output unchanged on failure; aliasing
   * allowed. Zero weights receive zero main-axis length. Empty input succeeds.
   */
  int mulib_layout_distribute (mulib_layout_rect_t parent, int direction,
                               double gap, const double *weights, size_t count,
                                mulib_layout_rect_t *out);
  typedef struct mulib_layout mulib_layout_t;
  typedef enum mulib_layout_wrap {
    MULIB_LAYOUT_WRAP_NONE, MULIB_LAYOUT_WRAP_WORD,
    MULIB_LAYOUT_WRAP_GRAPHEME, MULIB_LAYOUT_WRAP_WORD_GRAPHEME
  } mulib_layout_wrap_t;
  typedef enum mulib_layout_alignment {
    MULIB_LAYOUT_START, MULIB_LAYOUT_END, MULIB_LAYOUT_CENTER,
    MULIB_LAYOUT_LEFT, MULIB_LAYOUT_RIGHT
  } mulib_layout_alignment_t;
  /** @brief width=0 means unbounded; spacing is a positive metric multiplier.
   * base follows bidi: -1 auto/LTR, -2 auto/RTL, 0 LTR, 1 RTL. */
  typedef struct mulib_layout_options {
    double width, spacing;
    mulib_layout_wrap_t wrap;
    mulib_layout_alignment_t alignment;
    int base;
  } mulib_layout_options_t;
  /** @brief Owned visual line. start/length exclude newline; next includes it.
   * box is the logical advance box; baseline is absolute y, all pixels/y down.
   * Glyph positions are absolute within the result, ready for a renderer. */
  typedef struct mulib_layout_line {
    size_t start, length, next;
    mulib_layout_rect_t box;
    double baseline, ascent, descent;
    mulib_glyph_run_t run;
  } mulib_layout_line_t;
  typedef struct mulib_layout_result {
    char *text;
    size_t length;
    mulib_layout_line_t *lines;
    size_t count;
    double width, height;
  } mulib_layout_result_t;
  /** @brief Defaults: unbounded, word/grapheme wrap, start aligned, auto LTR. */
  mulib_layout_options_t mulib_layout_default(void);
  /** @brief Create a layout. Copies the ordered font pointer array and language;
   * borrows ICU/fonts, all scaled in 26.6 pixels. Keep these alive through compute
   * and rendering. Missing backend/coverage is an error, never fake metrics. */
  mulib_layout_t *mulib_layout_create(const mulib_pglayout_fontset_t *fonts);
  /** @brief Free layout (results remain valid). Does not release borrowed fonts. */
  void mulib_layout_destroy(mulib_layout_t *layout);
  /** @brief Copy strict UTF-8 including embedded NUL, maximum 16 MiB. 0/-1,
   * transactional. Empty input produces one metric-height empty line. */
  int mulib_layout_set_text(mulib_layout_t *layout, const char *text, size_t length);
  /** @brief Replace validated options transactionally. 0/-1. */
  int mulib_layout_configure(mulib_layout_t *layout, const mulib_layout_options_t *options);
  /** @brief Compute owned lines/glyphs/source mapping. Output must be zeroed;
   * 0/-1, unchanged on failure. Optional diagnostic has static message/byte offset.
   * Preserves spaces, hard breaks/blank lines; never splits graphemes. */
  int mulib_layout_compute(const mulib_layout_t *layout, mulib_layout_result_t *result,
                            mulib_xml_error_t *error);
  /** @brief Release/zero a result. */
  void mulib_layout_result_destroy(mulib_layout_result_t *result);
  /** @brief Nearest visual cluster edge at a point. Returns UTF-8 byte offset,
   * not an interior ligature caret; handles RTL and out-of-box points. 0/-1. */
  int mulib_layout_hit_test(const mulib_layout_result_t *result, double x, double y, size_t *offset);
  /** @brief Visit positioned glyphs with an origin translation. Callback returns
   * 0 to continue, nonzero to stop (returned unchanged). No rasterizer assumed. */
  typedef int (*mulib_layout_draw_fn)(void *context, const mulib_glyph_t *glyph);
  int mulib_layout_draw(const mulib_layout_result_t *result, double x, double y,
                        mulib_layout_draw_fn draw, void *context);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LAYOUT) && !defined(MULIB_LAYOUT_IMPLEMENTED)
#define MULIB_LAYOUT_IMPLEMENTED
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifndef MULIB_LAYOUT_MAX_MEASUREMENTS
#define MULIB_LAYOUT_MAX_MEASUREMENTS 100000
#endif
#ifndef MULIB_LAYOUT_MAX_SHAPED_BYTES
#define MULIB_LAYOUT_MAX_SHAPED_BYTES (64u*1024u*1024u)
#endif
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_layout {
  mulib_pglayout_fontset_t fonts;
  mulib_fontshape_t **owned_fonts;
  char *language, *text;
  size_t length;
  mulib_layout_options_t options;
};
mulib_layout_options_t mulib_layout_default(void) {
  mulib_layout_options_t o = {0, 1.2, MULIB_LAYOUT_WRAP_WORD_GRAPHEME, MULIB_LAYOUT_START, -1};
  return o;
}
void mulib_layout_destroy(mulib_layout_t *l) {
  if (l) { free(l->owned_fonts); free(l->language); free(l->text); free(l); }
}
mulib_layout_t *mulib_layout_create(const mulib_pglayout_fontset_t *fonts) {
  mulib_layout_t *l;
  size_t i, n;
  if (!fonts || !fonts->unicode || !fonts->fonts || !fonts->count ||
      fonts->count > SIZE_MAX / sizeof *fonts->fonts || fonts->base < -2 || fonts->base > 1) return NULL;
  for (i = 0; i < fonts->count; ++i) if (!fonts->fonts[i]) return NULL;
  l = (mulib_layout_t *)calloc(1, sizeof *l);
  if (!l) return NULL;
  l->fonts = *fonts; l->options = mulib_layout_default(); l->options.base = fonts->base;
  l->owned_fonts = (mulib_fontshape_t **)malloc(fonts->count * sizeof *l->owned_fonts);
  n = strlen(fonts->language ? fonts->language : "und");
  l->language = (char *)malloc(n+1); l->text = (char *)calloc(1, 1);
  if (!l->owned_fonts || !l->language || !l->text) { mulib_layout_destroy(l); return NULL; }
  memcpy(l->owned_fonts, fonts->fonts, fonts->count * sizeof *l->owned_fonts);
  memcpy(l->language, fonts->language ? fonts->language : "und", n+1);
  l->fonts.fonts = l->owned_fonts; l->fonts.language = l->language; return l;
}
int mulib_layout_set_text(mulib_layout_t *l, const char *s, size_t n) {
  char *p;
  if (!l || n > 16u*1024u*1024u || !mulib_utf8_validate((const unsigned char *)s, n)) return -1;
  p = (char *)malloc(n+1);
  if (!p) return -1;
  if (n) memcpy(p, s, n);
  p[n] = 0; free(l->text); l->text = p; l->length = n; return 0;
}
int mulib_layout_configure(mulib_layout_t *l, const mulib_layout_options_t *o) {
  if (!l || !o || !isfinite(o->width) || o->width < 0 || !isfinite(o->spacing) || o->spacing <= 0 ||
      o->wrap < MULIB_LAYOUT_WRAP_NONE || o->wrap > MULIB_LAYOUT_WRAP_WORD_GRAPHEME ||
      o->alignment < MULIB_LAYOUT_START || o->alignment > MULIB_LAYOUT_RIGHT || o->base < -2 || o->base > 1) return -1;
  l->options = *o; return 0;
}
void mulib_layout_result_destroy(mulib_layout_result_t *r) {
  size_t i;
  if (!r) return;
  for (i = 0; i < r->count; ++i) mulib_glyph_run_destroy(&r->lines[i].run);
  free(r->lines); free(r->text); memset(r, 0, sizeof *r);
}
static size_t mulib_layout_trim_(const char *s, size_t begin, size_t end) {
  if (end > begin && s[end-1] == '\n') { --end; if (end > begin && s[end-1] == '\r') --end; }
  else if (end > begin && (s[end-1] == '\r' || s[end-1] == '\v' || s[end-1] == '\f')) --end;
  else if (end-begin >= 2 && (unsigned char)s[end-2] == 0xc2 && (unsigned char)s[end-1] == 0x85) end -= 2;
  else if (end-begin >= 3 && (unsigned char)s[end-3] == 0xe2 && (unsigned char)s[end-2] == 0x80 &&
           ((unsigned char)s[end-1] == 0xa8 || (unsigned char)s[end-1] == 0xa9)) end -= 3;
  return end;
}
typedef struct mulib_layout_budget_ { size_t measurements, bytes; } mulib_layout_budget_;
static int mulib_layout_shape_(const mulib_layout_t *l, size_t start, size_t stop,
                               mulib_layout_budget_ *budget, mulib_glyph_run_t *run) {
  mulib_pglayout_fontset_t fonts = l->fonts;
  size_t bytes = l->length;
  if (start == bytes && bytes) {
    size_t last = bytes-1, used;
    uint32_t cp;
    while (last && ((unsigned char)l->text[last] & 0xc0) == 0x80) --last;
    if (!mulib_utf8_decode((const unsigned char *)l->text+last, bytes-last, &cp, &used)) return -1;
    if (cp == '\n' || cp == '\r' || cp == 0x85 || cp == 0x2029 || (cp >= 0x1c && cp <= 0x1e)) bytes = 0;
  }
  if (budget->measurements >= MULIB_LAYOUT_MAX_MEASUREMENTS ||
      bytes > MULIB_LAYOUT_MAX_SHAPED_BYTES-budget->bytes) return -1;
  ++budget->measurements; budget->bytes += bytes; fonts.base = l->options.base;
  /* Keep embedding/isolate context across U+2028 and other hard line breaks
   * that are not bidi paragraph separators. ICU selects the containing paragraph.
   * A trailing blank paragraph has no preceding paragraph direction to inherit. */
  return mulib_glyph_shape(&fonts, l->text, bytes, bytes ? start : 0, stop-start, run);
}
static int mulib_layout_line_(const mulib_layout_t *l, mulib_layout_result_t *r,
                              size_t start, size_t stop, size_t next, mulib_layout_budget_ *budget) {
  mulib_layout_line_t line;
  mulib_layout_line_t *lines;
  double ascent = 0, descent = 0, gap = 0, height, shift = 0;
  size_t i;
  memset(&line, 0, sizeof line);
  if (r->count >= 100000 || mulib_layout_shape_(l, start, stop, budget, &line.run)) return -1;
  for (i = 0; i < (line.run.count ? line.run.count : 1); ++i) {
    mulib_fontshape_metrics_t m;
    size_t f = line.run.count ? line.run.glyphs[i].font_index : 0;
    if (mulib_fontshape_metrics(l->fonts.fonts[f], &m)) goto fail;
    if (m.ascender/64.0 > ascent) ascent = m.ascender/64.0;
    if (-(double)m.descender/64.0 > descent) descent = -(double)m.descender/64.0;
    if (m.line_gap/64.0 > gap) gap = m.line_gap/64.0;
  }
  height = (ascent+descent+gap)*l->options.spacing;
  if (!isfinite(height) || height <= 0 || !isfinite(r->height+height)) goto fail;
  if (l->options.width > 0) {
    int right = l->options.alignment == MULIB_LAYOUT_RIGHT ||
      (l->options.alignment == MULIB_LAYOUT_START && (line.run.paragraph_level & 1)) ||
      (l->options.alignment == MULIB_LAYOUT_END && !(line.run.paragraph_level & 1));
    if (right) shift = l->options.width-line.run.advance;
    else if (l->options.alignment == MULIB_LAYOUT_CENTER) shift = (l->options.width-line.run.advance)/2;
  }
  line.start = start; line.length = stop-start; line.next = next;
  line.box.x = shift; line.box.y = r->height; line.box.width = line.run.advance; line.box.height = height;
  line.ascent = ascent; line.descent = descent;
  line.baseline = r->height+(height-ascent-descent)/2+ascent;
  for (i = 0; i < line.run.count; ++i) {
    line.run.glyphs[i].x += shift; line.run.glyphs[i].y += line.baseline;
    if (!isfinite(line.run.glyphs[i].x) || !isfinite(line.run.glyphs[i].y)) goto fail;
  }
  lines = (mulib_layout_line_t *)realloc(r->lines, (r->count+1)*sizeof *lines);
  if (!lines) goto fail;
  r->lines = lines; lines[r->count++] = line; r->height += height;
  if (line.run.advance > r->width) r->width = line.run.advance;
  return 0;
fail:
  mulib_glyph_run_destroy(&line.run); return -1;
}
int mulib_layout_compute(const mulib_layout_t *l, mulib_layout_result_t *out, mulib_xml_error_t *error) {
  mulib_layout_result_t r = {0};
  mulib_unibreak_boundary_t *b = NULL, *g = NULL;
  size_t nb = 0, ng = 0, p = 0, bi = 1, at = 0;
  mulib_layout_budget_ budget = {0};
  if (error) { error->offset = 0; error->message = NULL; }
  if (!l || !out || out->text || out->lines || out->count || out->length) goto fail;
  if (mulib_unibreak_find(l->fonts.unicode, MULIB_UNIBREAK_LINE, l->language, l->text, l->length, &b, &nb) ||
      mulib_unibreak_find(l->fonts.unicode, MULIB_UNIBREAK_GRAPHEME, l->language, l->text, l->length, &g, &ng)) goto fail;
  for (;;) {
    size_t hard = l->length, end, start;
    while (bi < nb && b[bi].offset <= p) ++bi;
    for (; bi < nb; ++bi) if (b[bi].status >= 100) { hard = b[bi++].offset; break; }
    end = mulib_layout_trim_(l->text, p, hard); start = p;
    do {
      size_t stop = end, i;
      at = start;
      if (start < end && l->options.width > 0 && l->options.wrap != MULIB_LAYOUT_WRAP_NONE) {
        const mulib_unibreak_boundary_t *c = l->options.wrap == MULIB_LAYOUT_WRAP_GRAPHEME ? g : b;
        size_t nc = c == g ? ng : nb;
        stop = start;
        for (i = 1; i < nc; ++i) {
          size_t candidate = c[i].offset < end ? c[i].offset : end;
          mulib_glyph_run_t run = {0};
          double width;
          if (candidate <= start) continue;
          if (mulib_layout_shape_(l, start, candidate, &budget, &run)) goto fail;
          width = run.advance; mulib_glyph_run_destroy(&run);
          if (width > l->options.width) {
            if (stop == start && l->options.wrap == MULIB_LAYOUT_WRAP_WORD) stop = candidate;
            break;
          }
          stop = candidate;
          if (candidate == end) break;
        }
        if (stop == start) {
          for (i = 1; i < ng && g[i].offset <= end; ++i) {
            mulib_glyph_run_t run = {0}; double width;
            if (g[i].offset <= start) continue;
            if (mulib_layout_shape_(l, start, g[i].offset, &budget, &run)) goto fail;
            width = run.advance; mulib_glyph_run_destroy(&run);
            if (stop > start && width > l->options.width) break;
            stop = g[i].offset;
            if (width > l->options.width) break;
          }
        }
        if (stop == start) goto fail;
      }
      if (mulib_layout_line_(l, &r, start, stop, stop == end ? hard : stop, &budget)) goto fail;
      start = stop;
    } while (start < end);
    if (hard == l->length && end == hard) break;
    p = hard;
    if (p == l->length) {
      if (mulib_layout_line_(l, &r, p, p, p, &budget)) goto fail;
      break;
    }
  }
  r.text = (char *)malloc(l->length+1);
  if (!r.text) goto fail;
  memcpy(r.text, l->text, l->length+1); r.length = l->length;
  free(b); free(g); *out = r; return 0;
fail:
  free(b); free(g); mulib_layout_result_destroy(&r);
  if (error) { error->offset = at; error->message = "invalid layout, missing font coverage/backend, allocation or work limit"; }
  return -1;
}
int mulib_layout_hit_test(const mulib_layout_result_t *r, double x, double y, size_t *offset) {
  const mulib_layout_line_t *line;
  size_t i = 0, j, best;
  double distance = HUGE_VAL, pen;
  if (!r || !r->lines || !r->count || !offset || !isfinite(x) || !isfinite(y)) return -1;
  while (i+1 < r->count && y >= r->lines[i].box.y+r->lines[i].box.height) ++i;
  line = &r->lines[i]; best = line->start; pen = line->box.x;
  for (j = 0; j < line->run.count;) {
    const mulib_glyph_t *v = &line->run.glyphs[j];
    size_t k = j; double end = pen, d;
    while (k < line->run.count && line->run.glyphs[k].start == v->start) end += line->run.glyphs[k++].advance;
    d = fabs(x-pen);
    if (d < distance) { distance = d; best = v->start + ((v->level & 1) ? v->length : 0); }
    d = fabs(x-end);
    if (d < distance) { distance = d; best = v->start + ((v->level & 1) ? 0 : v->length); }
    pen = end; j = k;
  }
  *offset = best; return 0;
}
int mulib_layout_draw(const mulib_layout_result_t *r, double x, double y, mulib_layout_draw_fn draw, void *ctx) {
  size_t i, j;
  if (!r || !draw || !isfinite(x) || !isfinite(y) || (r->count && !r->lines)) return -1;
  for (i = 0; i < r->count; ++i) for (j = 0; j < r->lines[i].run.count; ++j) {
    mulib_glyph_t g = r->lines[i].run.glyphs[j]; int status;
    g.x += x; g.y += y;
    if (!isfinite(g.x) || !isfinite(g.y)) return -1;
    status = draw(ctx, &g); if (status) return status;
  }
  return 0;
}
int
mulib_layout_distribute (mulib_layout_rect_t p, int dir, double gap,
                         const double *w, size_t n, mulib_layout_rect_t *out)
{
  double sum = 0, main, available, position = 0;
  size_t i;
  mulib_layout_rect_t *r;
  if ((dir != 0 && dir != 1) || !isfinite (p.x) || !isfinite (p.y)
      || !isfinite (p.width) || !isfinite (p.height) || p.width < 0
      || p.height < 0 || !isfinite (p.x + p.width)
      || !isfinite (p.y + p.height) || !isfinite (gap) || gap < 0 || (!w && n)
      || (!out && n) || n > SIZE_MAX / sizeof (*r))
    return 0;
  if (!n)
    return 1;
  for (i = 0; i < n; ++i)
    {
      if (!isfinite (w[i]) || w[i] < 0)
        return 0;
      sum += w[i];
    }
  if (!isfinite (sum) || sum <= 0)
    return 0;
  main = dir ? p.height : p.width;
  available = main - gap * (double)(n - 1);
  if (!isfinite (available) || available < 0)
    return 0;
  r = (mulib_layout_rect_t *)malloc (n * sizeof (*r));
  if (!r)
    return 0;
  for (i = 0; i < n; ++i)
    {
      double size = available * (w[i] / sum);
      r[i] = p;
      if (dir)
        {
          r[i].y += position;
          r[i].height = size;
        }
      else
        {
          r[i].x += position;
          r[i].width = size;
        }
      position += size;
      if (i + 1 < n)
        position += gap;
      if (!isfinite (position))
        {
          free (r);
          return 0;
        }
    }
  memcpy (out, r, n * sizeof (*r));
  free (r);
  return 1;
}
#ifdef __cplusplus
}
#endif
#endif
