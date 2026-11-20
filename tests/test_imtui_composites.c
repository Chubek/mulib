#define MULIB_IMPL_UTF8
#define MULIB_IMPL_GRID
#define MULIB_IMPL_VTERM
#define MULIB_IMPL_TERMKEY
#define MULIB_IMPL_IMTUI_CORE
#define MULIB_IMPL_IMTUI_DRAW
#define MULIB_IMPL_IMTUI_EDIT
#define MULIB_IMPL_IMTUI_LIST
#define MULIB_IMPL_IMTUI_TABLE
#define MULIB_IMPL_IMTUI_TABS
#include "mulib-imtui-edit.h"
#include "mulib-imtui-table.h"
#include "mulib-imtui-tabs.h"
#include <assert.h>
#include <string.h>

static void frame(mulib_imtui_t *ui, mulib_termkey_t *decoder, const char *bytes,
                  mulib_termkey_event_t events[32]) {
  size_t n = strlen(bytes), count = 0;
  int r;
  assert(mulib_termkey_feed(decoder, bytes, n) == n);
  while ((r = mulib_termkey_next(decoder, events + count, 0)) > 0) {
    ++count;
    assert(count < 32);
  }
  assert(r == 0 && mulib_imtui_begin(ui, events, count));
}

static mulib_term_cell_t cell(const mulib_imtui_t *ui, int x, int y) {
  return mulib_imtui_cells(ui)[(size_t)y * (size_t)mulib_imtui_bounds(ui).w + (size_t)x];
}

static void editor(void) {
  const mulib_imtui_rect_t r = {1, 1, 6, 2}, other = {1, 2, 6, 1};
  mulib_termkey_event_t events[32];
  mulib_imtui_edit_state_t s;
  mulib_term_style_t style = mulib_term_style_default();
  mulib_termkey_t decoder;
  mulib_imtui_t ui;
  char b[24] = "a\xc3\xa9\xf0\x9f\x98\x80z";
  size_t n = strlen(b);
  assert(mulib_imtui_init(&ui, 12, 5));
  mulib_termkey_init(&decoder);
  mulib_imtui_edit_init(&s, n);
  frame(&ui, &decoder, "", events);
  assert(!mulib_imtui_edit(&ui, 1, r, &s, b, &n, sizeof b, style, 1));
  assert(!mulib_imtui_interact(&ui, 2, other, 1));
  mulib_imtui_end(&ui);
  frame(&ui, &decoder, "\t\033[D\033[D\033[1;2D", events);
  assert(!mulib_imtui_edit(&ui, 1, r, &s, b, &n, sizeof b, style, 1));
  assert(s.cursor == 1 && s.anchor == 3 && s.first == 0);
  assert(cell(&ui, 2, 1).codepoint == 0xe9);
  assert((cell(&ui, 2, 1).style.attributes & (MULIB_TERM_INVERSE | MULIB_TERM_UNDERLINE))
         == (MULIB_TERM_INVERSE | MULIB_TERM_UNDERLINE));
  assert(!mulib_imtui_interact(&ui, 2, other, 1));
  mulib_imtui_end(&ui);
  frame(&ui, &decoder, "\xe4\xb8\xad \r\033[3~\x7f", events);
  assert(mulib_imtui_edit(&ui, 1, r, &s, b, &n, sizeof b, style, 1)
         == (MULIB_IMTUI_EDIT_CHANGED | MULIB_IMTUI_EDIT_SUBMITTED));
  assert(n == 5 && s.cursor == 4 && !strcmp(b, "a\xe4\xb8\xadz"));
  assert(!mulib_imtui_interact(&ui, 2, other, 1));
  mulib_imtui_end(&ui);
  frame(&ui, &decoder, "\033[H\033[3~\033[F\t", events);
  assert(mulib_imtui_edit(&ui, 1, r, &s, b, &n, sizeof b, style, 1) == MULIB_IMTUI_EDIT_CHANGED);
  assert(n == 4 && s.cursor == 4 && !strcmp(b, "\xe4\xb8\xadz"));
  assert(!(cell(&ui, 3, 1).style.attributes & MULIB_TERM_UNDERLINE));
  assert(mulib_imtui_interact(&ui, 2, other, 1) & MULIB_IMTUI_FOCUSED);
  mulib_imtui_end(&ui);
  strcpy(b, "a \xc3\xa9-xyz");
  n = strlen(b);
  mulib_imtui_edit_init(&s, n);
  frame(&ui, &decoder, "\033[<0;2;2M\033[<0;2;2m\033[1;5D\033[1;6D", events);
  assert(!mulib_imtui_edit(&ui, 1, r, &s, b, &n, sizeof b, style, 1));
  assert(s.cursor == 2 && s.anchor == 5);
  mulib_imtui_end(&ui);
  frame(&ui, &decoder, "\x7f", events);
  assert(mulib_imtui_edit(&ui, 1, r, &s, b, &n, sizeof b, style, 1) == MULIB_IMTUI_EDIT_CHANGED);
  assert(s.cursor == 2 && s.anchor == 2 && !strcmp(b, "a xyz"));
  mulib_imtui_end(&ui);
  mulib_imtui_free(&ui);
}

static void paste(void) {
  const mulib_imtui_rect_t r = {0, 0, 3, 1};
  mulib_termkey_event_t events[32];
  mulib_imtui_edit_state_t s;
  mulib_term_style_t style = mulib_term_style_default();
  mulib_termkey_t decoder;
  mulib_imtui_t ui;
  char b[8] = "12";
  size_t n = strlen(b);
  assert(mulib_imtui_init(&ui, 12, 4));
  mulib_termkey_init(&decoder);
  mulib_imtui_edit_init(&s, n);
  frame(&ui, &decoder, "", events);
  assert(!mulib_imtui_edit(&ui, 1, r, &s, b, &n, sizeof b, style, 1));
  mulib_imtui_end(&ui);
  frame(&ui, &decoder, "\t\033[200~\xe2\x82", events);
  assert(!mulib_imtui_edit(&ui, 1, r, &s, b, &n, sizeof b, style, 1));
  assert(n == 2 && !strcmp(b, "12"));
  mulib_imtui_end(&ui);
  frame(&ui, &decoder, "\xac\r", events);
  assert(mulib_imtui_edit(&ui, 1, r, &s, b, &n, sizeof b, style, 1) == MULIB_IMTUI_EDIT_CHANGED);
  assert(n == 6 && !strcmp(b, "12\xe2\x82\xac "));
  assert(s.first == 2 && cell(&ui, 0, 0).codepoint == 0x20ac);
  mulib_imtui_end(&ui);
  frame(&ui, &decoder, "\n\t\x01\xffQZ\033[201~", events);
  assert(mulib_imtui_edit(&ui, 1, r, &s, b, &n, sizeof b, style, 1)
         == (MULIB_IMTUI_EDIT_CHANGED | MULIB_IMTUI_EDIT_FULL));
  assert(n == 7 && !strcmp(b, "12\xe2\x82\xac  "));
  mulib_imtui_end(&ui);

  strcpy(b, "xx");
  n = 2;
  mulib_imtui_edit_init(&s, n);
  frame(&ui, &decoder, "\x01\033[200~\xf0\x9f\x98\x80x\033[201~", events);
  assert(mulib_imtui_edit(&ui, 1, r, &s, b, &n, 4, style, 1) == MULIB_IMTUI_EDIT_FULL);
  assert(n == 2 && s.cursor == 2 && s.anchor == 0 && !strcmp(b, "xx"));
  mulib_imtui_end(&ui);
  frame(&ui, &decoder, "Y", events);
  assert(mulib_imtui_edit(&ui, 1, r, &s, b, &n, 4, style, 1) == MULIB_IMTUI_EDIT_CHANGED);
  assert(n == 1 && !strcmp(b, "Y"));
  mulib_imtui_end(&ui);
  frame(&ui, &decoder, "\033[200~\xe2", events);
  assert(!mulib_imtui_edit(&ui, 1, r, &s, b, &n, sizeof b, style, 1));
  mulib_imtui_end(&ui);
  frame(&ui, &decoder, "\x82\xac\033[201~", events);
  /* The widget is absent while the paste suffix is routed to it. */
  mulib_imtui_end(&ui);
  frame(&ui, &decoder, "", events);
  assert(!mulib_imtui_edit(&ui, 1, r, &s, b, &n, sizeof b, style, 1));
  mulib_imtui_end(&ui);
  frame(&ui, &decoder, "\tZ", events);
  assert(mulib_imtui_edit(&ui, 1, r, &s, b, &n, sizeof b, style, 1) == MULIB_IMTUI_EDIT_CHANGED);
  assert(n == 2 && !strcmp(b, "YZ"));
  mulib_imtui_end(&ui);
  mulib_imtui_free(&ui);
}

typedef struct calls {
  size_t count, row[16], column[16];
} calls_t;

static const char *list_label(void *user, size_t row, size_t *length) {
  calls_t *log = (calls_t *)user;
  assert(log->count < sizeof log->row / sizeof *log->row);
  log->row[log->count++] = row;
  *length = 11;
  return "row-abcdefg";
}

static const char *table_cell(void *user, size_t row, size_t col, size_t *length) {
  calls_t *log = (calls_t *)user;
  assert(log->count < sizeof log->row / sizeof *log->row);
  log->row[log->count] = row;
  log->column[log->count++] = col;
  *length = 4;
  return col == 0 ? "1111" : "2222";
}

static void virtual_rows(void) {
  const mulib_imtui_rect_t r = {2, 2, 4, 3};
  const mulib_imtui_rect_t window = {2, 3, 4, 1};
  const mulib_imtui_rect_t table = {4, 0, 6, 4};
  const mulib_imtui_rect_t table_clip = {6, 0, 9, 3};
  const mulib_imtui_table_column_t columns[] = {
    {"head", 4, 2}, {"hidden", 6, 0}, {"xyzabc", 6, 4}, {"off", 3, 10}
  };
  mulib_termkey_event_t events[32];
  mulib_imtui_list_state_t s, t;
  mulib_term_style_t style = mulib_term_style_default();
  mulib_termkey_t decoder;
  mulib_imtui_t ui;
  calls_t log = {0};
  assert(mulib_imtui_init(&ui, 16, 8));
  mulib_termkey_init(&decoder);
  mulib_imtui_list_init(&s);
  mulib_imtui_list_init(&t);
  frame(&ui, &decoder, "", events);
  assert(!mulib_imtui_list(&ui, 1, r, &s, SIZE_MAX, list_label, &log, style, 1));
  assert(log.count == 3 && log.row[0] == 0 && log.row[2] == 2);
  assert(cell(&ui, 5, 2).codepoint == '-' && cell(&ui, 6, 2).codepoint == ' ');
  mulib_imtui_end(&ui);
  log.count = 0;
  frame(&ui, &decoder, "\t\033[F\r", events);
  assert(mulib_imtui_list(&ui, 1, r, &s, SIZE_MAX, list_label, &log, style, 1)
         == (MULIB_IMTUI_LIST_CHANGED | MULIB_IMTUI_LIST_ACTIVATED));
  assert(s.selected == SIZE_MAX - 1 && s.activated == s.selected && s.first == SIZE_MAX - 3);
  assert(log.count == 3 && log.row[0] == s.first && log.row[2] == s.selected);
  assert(cell(&ui, 2, 4).style.attributes & MULIB_TERM_INVERSE);
  mulib_imtui_end(&ui);
  log.count = 0;
  frame(&ui, &decoder, "\033[5~", events);
  assert(mulib_imtui_clip_push(&ui, window));
  assert(mulib_imtui_list(&ui, 1, r, &s, SIZE_MAX, list_label, &log, style, 1)
         == MULIB_IMTUI_LIST_CHANGED);
  assert(s.selected == SIZE_MAX - 4 && s.first == s.selected && s.activated == MULIB_IMTUI_LIST_NONE);
  assert(log.count == 1 && log.row[0] == s.first + 1 && cell(&ui, 2, 2).codepoint == ' ');
  mulib_imtui_clip_pop(&ui);
  mulib_imtui_end(&ui);
  log.count = 0;
  frame(&ui, &decoder, "", events);
  assert(!mulib_imtui_list(&ui, 1, r, &s, 1, list_label, &log, style, 0));
  assert(s.first == 0 && s.selected == MULIB_IMTUI_LIST_NONE && log.count == 1);
  log.count = 0;
  assert(mulib_imtui_clip_push(&ui, table_clip));
  assert(!mulib_imtui_table(&ui, 2, table, &t, SIZE_MAX, columns, 4, table_cell, &log, style, 1));
  assert(log.count == 2 && log.row[0] == 0 && log.row[1] == 1);
  assert(log.column[0] == 2 && log.column[1] == 2);
  assert(cell(&ui, 7, 0).codepoint == 'x' && cell(&ui, 10, 0).codepoint == ' ');
  assert(cell(&ui, 7, 1).codepoint == '2' && cell(&ui, 7, 3).codepoint == ' ');
  mulib_imtui_clip_pop(&ui);
  mulib_imtui_end(&ui);
  log.count = 0;
  frame(&ui, &decoder, "\t\033[F", events);
  assert(mulib_imtui_table(&ui, 2, table, &t, SIZE_MAX, columns, 4, table_cell, &log, style, 1)
         == MULIB_IMTUI_LIST_CHANGED);
  assert(log.count == 6 && log.row[0] == SIZE_MAX - 3 && log.row[5] == SIZE_MAX - 1);
  assert(cell(&ui, 4, 0).codepoint == 'h' && cell(&ui, 6, 0).codepoint == ' ');
  assert(t.selected == SIZE_MAX - 1 && cell(&ui, 4, 3).style.attributes & MULIB_TERM_INVERSE);
  mulib_imtui_end(&ui);
  mulib_imtui_free(&ui);
}

static void tabs(void) {
  const mulib_imtui_rect_t r = {1, 0, 10, 2};
  const mulib_imtui_tab_t items[] = {
    {"A", 1, 1}, {"x", 1, 0}, {"Wide panel", 10, 1}, {"Z", 1, 1}
  };
  mulib_termkey_event_t events[32];
  mulib_imtui_tabs_state_t s;
  mulib_term_style_t style = mulib_term_style_default();
  mulib_termkey_t decoder;
  mulib_imtui_t ui;
  assert(mulib_imtui_init(&ui, 12, 4));
  mulib_termkey_init(&decoder);
  mulib_imtui_tabs_init(&s, 0);
  frame(&ui, &decoder, "", events);
  assert(!mulib_imtui_tabs(&ui, 1, r, &s, items, 4, style, 1));
  assert(cell(&ui, 2, 0).codepoint == 'A');
  assert(cell(&ui, 6, 0).codepoint == 'x' && cell(&ui, 6, 0).style.attributes & MULIB_TERM_DIM);
  mulib_imtui_end(&ui);
  frame(&ui, &decoder, "\t\033[C", events);
  assert(mulib_imtui_tabs(&ui, 1, r, &s, items, 4, style, 1));
  assert(s.selected == 2 && s.first == 2 && cell(&ui, 2, 0).codepoint == 'W');
  assert(cell(&ui, 2, 0).style.attributes & MULIB_TERM_INVERSE);
  assert(cell(&ui, 11, 0).codepoint == ' ' && cell(&ui, 2, 1).codepoint == ' ');
  mulib_imtui_end(&ui);
  frame(&ui, &decoder, "\033[F\033[H", events);
  assert(mulib_imtui_tabs(&ui, 1, r, &s, items, 4, style, 1));
  assert(s.selected == 0 && s.first == 0);
  mulib_imtui_end(&ui);
  mulib_imtui_free(&ui);
}

int main(void) {
  editor();
  paste();
  virtual_rows();
  tabs();
  return 0;
}
