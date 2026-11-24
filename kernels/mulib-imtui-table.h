/**
 * @file mulib-imtui-table.h
 * @brief Header and virtual rows for caller-owned tabular data.
 *
 * Define MULIB_IMPL_IMTUI_TABLE in one translation unit. Requires the ImTUI
 * list implementation. No sorting or backing store is imposed on the caller.
 */
#ifndef MULIB_IMTUI_TABLE_H
#define MULIB_IMTUI_TABLE_H
#include "mulib-imtui-list.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_imtui_table_column {
  const char *label;
  size_t length;
  int width; /**< Content width in cells; <= 0 hides a column. */
} mulib_imtui_table_column_t;

/** Return a borrowed UTF-8 cell label; NULL is empty. */
typedef const char *(*mulib_imtui_table_cell_fn)(void *, size_t row,
                                                  size_t column, size_t *length);

/**
 * Keep the header fixed on the first line; use list interaction/state for
 * the remaining lines. Only visible data cells request content. Columns
 * have a one-cell gap; the last visible column may be clipped on either side.
 * Pass NULL/zero columns for an empty view.
 */
unsigned mulib_imtui_table(mulib_imtui_t *, mulib_imtui_id_t, mulib_imtui_rect_t,
                          mulib_imtui_list_state_t *, size_t rows,
                          const mulib_imtui_table_column_t *, size_t columns,
                          mulib_imtui_table_cell_fn, void *, mulib_term_style_t,
                          int enabled);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMTUI_TABLE_H */

#if defined(MULIB_IMPL_IMTUI_TABLE) && !defined(MULIB_IMTUI_TABLE_IMPLEMENTED)
#define MULIB_IMTUI_TABLE_IMPLEMENTED
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_imtui_table_context_ {
  const mulib_imtui_table_column_t *columns;
  size_t count;
  mulib_imtui_table_cell_fn cell;
  void *user;
} mulib_imtui_table_context_;

static void mulib_imtui_table_row_(mulib_imtui_t *ui, mulib_imtui_rect_t row,
                                   size_t index, mulib_term_style_t style, void *data) {
  const mulib_imtui_table_context_ *t = (const mulib_imtui_table_context_ *)data;
  mulib_imtui_rect_t clip = mulib_imtui_clip(ui);
  int64_t right = (int64_t)row.x + row.w, x = row.x;
  int64_t clip_right = (int64_t)clip.x + clip.w;
  size_t i;
  if (clip.w <= 0 || row.y < clip.y || (int64_t)row.y >= (int64_t)clip.y + clip.h)
    return;
  if (clip_right < right) right = clip_right;
  for (i = 0; i < t->count && x < right; ++i) {
    const mulib_imtui_table_column_t *col = t->columns + i;
    int64_t far = x + col->width;
    if (col->width <= 0) continue;
    if (far > clip.x) {
      mulib_imtui_rect_t part = {(int)x, row.y, col->width, 1};
      if (far > right) part.w = (int)(right - x);
      if (mulib_imtui_clip_push(ui, part)) {
        const char *text = NULL;
        size_t n = 0;
        if (index == MULIB_IMTUI_LIST_NONE) {
          text = col->label; n = col->length;
        } else if (t->cell) {
          text = t->cell(t->user, index, i, &n);
        }
        if (text) mulib_imtui_text(ui, part.x, part.y, text, n, style);
        mulib_imtui_clip_pop(ui);
      }
    }
    x = far + 1;
  }
}

unsigned mulib_imtui_table(mulib_imtui_t *ui, mulib_imtui_id_t id,
                          mulib_imtui_rect_t rect, mulib_imtui_list_state_t *state,
                          size_t rows, const mulib_imtui_table_column_t *columns,
                          size_t count, mulib_imtui_table_cell_fn cell, void *user,
                          mulib_term_style_t style, int enabled) {
  mulib_imtui_table_context_ t = {columns, columns ? count : 0, cell, user};
  mulib_imtui_rect_t header = {rect.x, rect.y, rect.w, 1};
  mulib_term_cell_t blank = {' ', style};
  if (!ui || rect.w <= 0 || rect.h <= 0)
    return mulib_imtui_list_view(ui, id, rect, state, rows, NULL, NULL, style, enabled);
  blank.style.attributes |= MULIB_TERM_BOLD;
  if (!enabled) blank.style.attributes |= MULIB_TERM_DIM;
  mulib_imtui_fill(ui, header, blank);
  mulib_imtui_table_row_(ui, header, MULIB_IMTUI_LIST_NONE, blank.style, &t);
  --rect.h;
  if (rect.y < INT_MAX) ++rect.y;
  else rect.h = 0;
  return mulib_imtui_list_view(ui, id, rect, state, t.count ? rows : 0,
                               mulib_imtui_table_row_, &t, style, enabled);
}

#ifdef __cplusplus
}
#endif
#endif /* implementation */
