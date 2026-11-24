/** @file mulib-imtui.h
 * @brief Immediate-mode terminal widgets, layout, rendering, and input.
 * Define MULIB_IMPL_IMTUI for the headless snapshot helper. Dependencies never
 * enable their implementation switches implicitly.
 */
#ifndef MULIB_IMTUI_H
#define MULIB_IMTUI_H
#include "mulib-imtui-core.h"
#include "mulib-imtui-draw.h"
#include "mulib-imtui-edit.h"
#include "mulib-imtui-layout.h"
#include "mulib-imtui-list.h"
#include "mulib-imtui-scroll.h"
#include "mulib-imtui-render.h"
#include "mulib-imtui-syntax.h"
#include "mulib-imtui-table.h"
#include "mulib-imtui-tabs.h"
#include "mulib-imtui-terminal.h"
#include "mulib-imtui-widgets.h"
#include "mulib-imtui-mune.h"
#include "mulib-strbuf.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Append a plain UTF-8 snapshot of the cell buffer, including a newline after
 * each row. No terminal escapes, colors or screen updates. Returns 1/0 and
 * leaves output unchanged on failure. Useful in tests and log-oriented UIs. */
int mulib_imtui_snapshot(const mulib_imtui_t *, mulib_strbuf_t *out);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_IMTUI) && !defined(MULIB_IMTUI_IMPLEMENTED)
#define MULIB_IMTUI_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif

int
mulib_imtui_snapshot(const mulib_imtui_t *ui, mulib_strbuf_t *out)
{
  if (!ui || !out) return 0;
  mulib_imtui_rect_t bounds = mulib_imtui_bounds(ui);
  const mulib_term_cell_t *cells = mulib_imtui_cells(ui);
  if (!cells || bounds.w <= 0 || bounds.h <= 0) return 0;
  mulib_strbuf_t tmp = { NULL, 0, 0 };
  int ok = 1;
  for (int y = 0; ok && y < bounds.h; ++y) {
    for (int x = 0; ok && x < bounds.w; ++x, ++cells) {
      unsigned char bytes[4];
      uint32_t ch = mulib_term_cell_valid(*cells) ? cells->codepoint : 0xfffd;
      size_t n = mulib_utf8_encode(ch, bytes, sizeof bytes);
      ok = n && mulib_strbuf_append_n(&tmp, (const char *)bytes, n);
    }
    if (ok) ok = mulib_strbuf_putc(&tmp, '\n');
  }
  if (ok) ok = mulib_strbuf_append_n(out, tmp.data, tmp.length);
  mulib_strbuf_free(&tmp);
  return ok;
}

#ifdef __cplusplus
}
#endif
#endif
