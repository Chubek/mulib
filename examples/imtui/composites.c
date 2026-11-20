/* Headless tabs, editor, and visible rows backed by caller-owned data. */
#define MULIB_IMPL_GRID
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_VTERM
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_IMTUI
#define MULIB_IMPL_IMTUI_CORE
#define MULIB_IMPL_IMTUI_DRAW
#define MULIB_IMPL_IMTUI_EDIT
#define MULIB_IMPL_IMTUI_LIST
#define MULIB_IMPL_IMTUI_SCROLL
#define MULIB_IMPL_IMTUI_TABLE
#define MULIB_IMPL_IMTUI_TABS
#include "mulib-imtui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *
get_cell(void *context, size_t row, size_t col, size_t *length)
{
  (void)context;
  static const char *const entries[][2] = {
    { "Core", "Ready" }, { "Renderer", "Ready" }, { "Mune", "Ready" }
  };
  const char *s = entries[row][col];
  *length = strlen(s);
  return s;
}

int
main(void)
{
  mulib_imtui_t ui;
  mulib_imtui_tabs_state_t tabs;
  mulib_imtui_list_state_t list;
  mulib_imtui_edit_state_t edit;
  mulib_strbuf_t snapshot = { NULL, 0, 0 };
  char text[32] = "all";
  size_t len = strlen(text);
  const mulib_imtui_tab_t labels[] = { { "Overview", 8, 1 }, { "Details", 7, 1 } };
  const mulib_imtui_table_column_t columns[] = { { "Kernel", 6, 18 }, { "Status", 6, 10 } };
  mulib_term_style_t style = mulib_term_style_default();
  mulib_imtui_tabs_init(&tabs, 0);
  mulib_imtui_list_init(&list);
  mulib_imtui_edit_init(&edit, len);
  if (!mulib_imtui_init(&ui, 33, 8)) return EXIT_FAILURE;
  int ok = mulib_imtui_begin(&ui, NULL, 0);
  if (ok) {
    (void)mulib_imtui_tabs(&ui, mulib_imtui_id(&ui, "tabs", 4),
      (mulib_imtui_rect_t){ 1, 0, 31, 1 }, &tabs, labels, 2, style, 1);
    ok = mulib_imtui_text(&ui, 1, 2, "Filter: ", 8, style);
    (void)mulib_imtui_edit(&ui, mulib_imtui_id(&ui, "filter", 6),
      (mulib_imtui_rect_t){ 9, 2, 21, 1 }, &edit, text, &len, sizeof text, style, 1);
    mulib_imtui_scroll_view_t viewport;
    if (ok && mulib_imtui_scroll_viewport((mulib_imtui_rect_t){ 1, 4, 31, 4 },
        31, 4, 0, 0, &viewport) && mulib_imtui_scroll_begin(&ui, &viewport)) {
      (void)mulib_imtui_table(&ui, mulib_imtui_id(&ui, "table", 5), viewport.viewport,
        &list, 3, columns, 2, get_cell, NULL, style, 1);
      mulib_imtui_scroll_end(&ui);
    } else ok = 0;
    mulib_imtui_end(&ui);
  }
  ok = ok && mulib_imtui_snapshot(&ui, &snapshot)
    && fwrite(snapshot.data, 1, snapshot.length, stdout) == snapshot.length;
  mulib_strbuf_free(&snapshot);
  mulib_imtui_free(&ui);
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
