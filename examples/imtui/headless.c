/* A deterministic ImTUI frame rendered without a terminal. */
#define MULIB_IMPL_IMTUI
#define MULIB_IMPL_IMTUI_CORE
#define MULIB_IMPL_IMTUI_DRAW
#define MULIB_IMPL_IMTUI_LAYOUT
#define MULIB_IMPL_IMTUI_WIDGETS
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_GRID
#define MULIB_IMPL_VTERM
#define MULIB_IMPL_UTF8
#include "mulib-imtui.h"
#include <stdio.h>
#include <stdlib.h>

int
main(void)
{
  mulib_imtui_t ui;
  mulib_strbuf_t output = { NULL, 0, 0 };
  mulib_term_style_t style = mulib_term_style_default();
  const mulib_imtui_rect_t form = { 2, 1, 24, 3 };
  mulib_imtui_rect_t controls;
  int checked = 1;
  if (!mulib_imtui_init(&ui, 28, 5)) return EXIT_FAILURE;
  int ok = mulib_imtui_begin(&ui, NULL, 0)
    && mulib_imtui_box(&ui, mulib_imtui_bounds(&ui), style, 0)
    && mulib_imtui_text(&ui, form.x, form.y, "ImTUI settings", 14, style)
    && mulib_imtui_layout_column(form, 0, 3, 1, &controls);
  if (ok) {
    mulib_imtui_checkbox(&ui, mulib_imtui_id(&ui, "sound", 5),
                          controls, "Sound", 5, &checked, 1);
    ok = mulib_imtui_layout_column(form, 0, 3, 2, &controls);
  }
  if (ok)
    mulib_imtui_button(&ui, mulib_imtui_id(&ui, "save", 4), controls, "Save", 4, 1);
  mulib_imtui_end(&ui);
  ok = ok && mulib_imtui_snapshot(&ui, &output)
    && fwrite(output.data, 1, output.length, stdout) == output.length;
  mulib_strbuf_free(&output);
  mulib_imtui_free(&ui);
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
