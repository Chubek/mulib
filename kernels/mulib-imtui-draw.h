/**
 * @file mulib-imtui-draw.h
 * @brief Clipped cell primitives and single-row UTF-8 labels.
 *
 * Define MULIB_IMPL_IMTUI_DRAW in one translation unit. Requires the ImTUI core
 * and shared UTF-8 and terminal cell implementations.
 */
#ifndef MULIB_IMTUI_DRAW_H
#define MULIB_IMTUI_DRAW_H
#include "mulib-imtui-core.h"
#ifdef __cplusplus
extern "C" {
#endif

/** @brief Fill a rectangle clipped to the screen and current clip.
 * Draw between begin/end. Valid empty/fully clipped draws succeed.
 * Drawing calls return 1/0. */
int mulib_imtui_fill(mulib_imtui_t *, mulib_imtui_rect_t, mulib_term_cell_t);
/** @brief Draw rightward/downward from (x,y), excluding the far endpoint. */
int mulib_imtui_hline(mulib_imtui_t *, int x, int y, int length, mulib_term_cell_t);
int mulib_imtui_vline(mulib_imtui_t *, int x, int y, int length, mulib_term_cell_t);
/** @brief Draw a clipped border, without changing its interior. Unicode
 * selects light box drawing instead of ASCII. A one-cell-wide or high box
 * becomes a line, and a 1x1 box becomes '+' (ASCII) or U+253C (Unicode). */
int mulib_imtui_box(mulib_imtui_t *, mulib_imtui_rect_t, mulib_term_style_t, int unicode);
/** @brief Measure a single-row label using one cell per Unicode scalar, not
 * wcwidth, combining or grapheme segmentation. Each malformed byte and each
 * C0, DEL or C1 control (including newline and tab) count as one U+FFFD cell.
 * NULL text has width zero. */
size_t mulib_imtui_text_width(const char *, size_t length);
/** @brief Draw a length-delimited single-row label with the same replacement
 * and width rules as text_width. Embedded NUL is not a terminator. NULL text
 * is only allowed for zero length. */
int mulib_imtui_text(mulib_imtui_t *, int x, int y, const char *, size_t length,
                    mulib_term_style_t);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_IMTUI_DRAW) && !defined(MULIB_IMTUI_DRAW_IMPLEMENTED)
#define MULIB_IMTUI_DRAW_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif

/* Widen before taking far edges: rects and endpoints are allowed to lie
 * arbitrarily far outside the viewport. Only narrow coordinates after clipping. */
static int
mulib_imtui_draw_area_(mulib_imtui_t *ui, mulib_imtui_rect_t clip,
                      int64_t x0, int64_t y0, int64_t x1, int64_t y1,
                      mulib_term_cell_t cell)
{
  int64_t x, y, right = (int64_t)clip.x + clip.w, bottom = (int64_t)clip.y + clip.h;
  if (x0 < clip.x) x0 = clip.x;
  if (y0 < clip.y) y0 = clip.y;
  if (x1 > right) x1 = right;
  if (y1 > bottom) y1 = bottom;
  if (x1 <= x0 || y1 <= y0) return 1;
  for (y = y0; y < y1; ++y)
    for (x = x0; x < x1; ++x)
      if (!mulib_imtui_put(ui, (int)x, (int)y, cell)) return 0;
  return 1;
}

int
mulib_imtui_fill(mulib_imtui_t *ui, mulib_imtui_rect_t rect, mulib_term_cell_t cell)
{
  if (!ui || !ui->in_frame || !mulib_term_cell_valid(cell)) return 0;
  return mulib_imtui_draw_area_(ui, mulib_imtui_clip(ui), rect.x, rect.y,
                               (int64_t)rect.x + rect.w, (int64_t)rect.y + rect.h, cell);
}

int
mulib_imtui_hline(mulib_imtui_t *ui, int x, int y, int n, mulib_term_cell_t cell)
{
  mulib_imtui_rect_t rect = {x, y, n, 1};
  return mulib_imtui_fill(ui, rect, cell);
}

int
mulib_imtui_vline(mulib_imtui_t *ui, int x, int y, int n, mulib_term_cell_t cell)
{
  mulib_imtui_rect_t rect = {x, y, 1, n};
  return mulib_imtui_fill(ui, rect, cell);
}

int
mulib_imtui_box(mulib_imtui_t *ui, mulib_imtui_rect_t rect, mulib_term_style_t style,
               int unicode)
{
  static const uint32_t borders[2][7] = {
    {'+', '+', '+', '+', '-', '|', '+'},
    {0x250c, 0x2510, 0x2514, 0x2518, 0x2500, 0x2502, 0x253c}
  };
  const uint32_t *glyph = borders[!!unicode];
  mulib_imtui_rect_t clip;
  mulib_term_cell_t cell = {' ', style};
  int64_t left = rect.x, top = rect.y, right, bottom;
  if (!ui || !ui->in_frame || !mulib_term_cell_valid(cell)) return 0;
  if (rect.w <= 0 || rect.h <= 0) return 1;
  clip = mulib_imtui_clip(ui);
  right = left + rect.w - 1;
  bottom = top + rect.h - 1;
  if (rect.w == 1 || rect.h == 1)
    {
      cell.codepoint = rect.w != 1 ? glyph[4] : rect.h != 1 ? glyph[5] : glyph[6];
      return mulib_imtui_draw_area_(ui, clip, left, top, right + 1, bottom + 1, cell);
    }
  cell.codepoint = glyph[4];
  if (!mulib_imtui_draw_area_(ui, clip, left + 1, top, right, top + 1, cell)
      || !mulib_imtui_draw_area_(ui, clip, left + 1, bottom, right, bottom + 1, cell))
    return 0;
  cell.codepoint = glyph[5];
  if (!mulib_imtui_draw_area_(ui, clip, left, top + 1, left + 1, bottom, cell)
      || !mulib_imtui_draw_area_(ui, clip, right, top + 1, right + 1, bottom, cell))
    return 0;
  cell.codepoint = glyph[0];
  if (!mulib_imtui_draw_area_(ui, clip, left, top, left + 1, top + 1, cell)) return 0;
  cell.codepoint = glyph[1];
  if (!mulib_imtui_draw_area_(ui, clip, right, top, right + 1, top + 1, cell)) return 0;
  cell.codepoint = glyph[2];
  if (!mulib_imtui_draw_area_(ui, clip, left, bottom, left + 1, bottom + 1, cell)) return 0;
  cell.codepoint = glyph[3];
  return mulib_imtui_draw_area_(ui, clip, right, bottom, right + 1, bottom + 1, cell);
}

static size_t
mulib_imtui_draw_decode_(const char *text, size_t n, uint32_t *codepoint)
{
  size_t used;
  if (!mulib_utf8_decode((const unsigned char *)text, n, codepoint, &used))
    {
      *codepoint = 0xfffd;
      return 1;
    }
  if (*codepoint < 32 || (*codepoint >= 127 && *codepoint < 160))
    *codepoint = 0xfffd;
  return used;
}

size_t
mulib_imtui_text_width(const char *text, size_t n)
{
  size_t at = 0, width = 0;
  uint32_t codepoint;
  if (text)
    while (at < n)
      {
        at += mulib_imtui_draw_decode_(text + at, n - at, &codepoint);
        ++width;
      }
  return width;
}

int
mulib_imtui_text(mulib_imtui_t *ui, int x, int y, const char *text, size_t n,
                mulib_term_style_t style)
{
  mulib_term_cell_t cell = {' ', style};
  mulib_imtui_rect_t clip;
  size_t at = 0;
  int64_t column = x, right;
  if ((!text && n) || !ui || !ui->in_frame || !mulib_term_cell_valid(cell)) return 0;
  clip = mulib_imtui_clip(ui);
  if (clip.w <= 0 || y < clip.y || (int64_t)y >= (int64_t)clip.y + clip.h) return 1;
  right = (int64_t)clip.x + clip.w;
  while (at < n && column < right)
    {
      at += mulib_imtui_draw_decode_(text + at, n - at, &cell.codepoint);
      if (column >= clip.x && !mulib_imtui_put(ui, (int)column, y, cell)) return 0;
      ++column;
    }
  return 1;
}

#ifdef __cplusplus
}
#endif
#endif
