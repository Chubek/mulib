#include <assert.h>
#include <limits.h>
#include <string.h>

#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_GRID
#define MULIB_IMPL_VTERM
#define MULIB_IMPL_TERMINFO
#define MULIB_IMPL_XML
#define MULIB_IMPL_CSS
#define MULIB_IMPL_TERMSTYLE
#define MULIB_IMPL_IMTUI_CORE
#define MULIB_IMPL_IMTUI_DRAW
#define MULIB_IMPL_IMTUI_RENDER
#include "mulib-imtui-draw.h"
#include "mulib-imtui-render.h"

#define REPLACEMENT "\xef\xbf\xbd"

static void
test_drawing(void)
{
  static const char label[] = "A\xc3\xa9\t\033\xc2\x9b\xffZ";
  static const char snapshot[] =
    "+-------+\n"
    "|.##!##.|\n"
    "|.\xc3\xa9" REPLACEMENT REPLACEMENT REPLACEMENT REPLACEMENT "\xe2\x94\x82|\n"
    "|\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80+\xe2\x94\xbc.\xe2\x94\x82|\n"
    "+-------+\n";
  mulib_imtui_t ui;
  mulib_strbuf_t out;
  mulib_term_style_t style = mulib_term_style_default();
  mulib_term_cell_t dot = {'.', style}, hash = {'#', style}, bang = {'!', style};
  mulib_term_cell_t bad = {27, style};
  assert(mulib_imtui_init(&ui, 9, 5));
  assert(!mulib_imtui_fill(&ui, (mulib_imtui_rect_t){0, 0, 1, 1}, dot));
  assert(mulib_imtui_begin(&ui, NULL, 0));
  assert(mulib_imtui_box(&ui, mulib_imtui_bounds(&ui), style, 0));
  assert(!mulib_imtui_fill(&ui, (mulib_imtui_rect_t){1, 1, 7, 3}, bad));
  assert(mulib_imtui_clip_push(&ui, (mulib_imtui_rect_t){1, 1, 7, 3}));
  assert(mulib_imtui_fill(&ui, (mulib_imtui_rect_t){-1, -1, INT_MAX, INT_MAX}, dot));
  assert(mulib_imtui_clip_push(&ui, (mulib_imtui_rect_t){2, 1, 5, 2}));
  assert(mulib_imtui_hline(&ui, INT_MIN + 10, 1, INT_MAX, hash));
  assert(mulib_imtui_vline(&ui, 4, INT_MIN + 10, INT_MAX, bang));
  assert(mulib_imtui_text_width(label, sizeof(label) - 1) == 7);
  assert(mulib_imtui_text_width("e\xcc\x81\xe4\xb8\xad", 6) == 3);
  assert(mulib_imtui_text_width("\0\r\n\177\xe2\x80", 6) == 6);
  assert(mulib_imtui_text_width(NULL, 9) == 0);
  assert(mulib_imtui_text(&ui, 1, 2, label, sizeof(label) - 1, style));
  mulib_imtui_clip_pop(&ui);
  assert(mulib_imtui_box(&ui, (mulib_imtui_rect_t){1, 3, 3, 1}, style, 1));
  assert(mulib_imtui_box(&ui, (mulib_imtui_rect_t){4, 3, 1, 1}, style, 0));
  assert(mulib_imtui_box(&ui, (mulib_imtui_rect_t){5, 3, 1, 1}, style, 1));
  assert(mulib_imtui_box(&ui, (mulib_imtui_rect_t){7, 2, 1, 2}, style, 1));
  mulib_imtui_clip_pop(&ui);
  assert(mulib_imtui_clip_push(&ui, (mulib_imtui_rect_t){INT_MAX, 1, INT_MAX, 3}));
  assert(mulib_imtui_box(&ui, (mulib_imtui_rect_t){0, 0, INT_MAX, INT_MAX}, style, 0));
  assert(mulib_imtui_text(&ui, 0, 0, label, sizeof(label) - 1, style));
  assert(mulib_imtui_text(&ui, 0, 0, NULL, 0, style));
  assert(!mulib_imtui_text(&ui, 0, 0, NULL, 1, style));
  mulib_imtui_clip_pop(&ui);
  mulib_imtui_end(&ui);
  mulib_strbuf_init(&out);
  assert(mulib_imtui_render(NULL, &ui, NULL, MULIB_TERMSTYLE_PLAIN, &out));
  assert(out.length == sizeof(snapshot) - 1 && !strcmp(out.data, snapshot));
  mulib_strbuf_free(&out);

  /* The original corners stay put when a side is clipped away. */
  assert(mulib_imtui_begin(&ui, NULL, 0));
  assert(mulib_imtui_box(&ui, (mulib_imtui_rect_t){-1, 0, 4, 3}, style, 1));
  assert(mulib_imtui_box(&ui, (mulib_imtui_rect_t){INT_MAX, 1, INT_MAX, INT_MAX}, style, 1));
  assert(mulib_imtui_text(&ui, -1, 3, "x\0\nz", 4, style));
  assert(mulib_imtui_cells(&ui)[0].codepoint == 0x2500);
  assert(mulib_imtui_cells(&ui)[2].codepoint == 0x2510);
  assert(mulib_imtui_cells(&ui)[11].codepoint == 0x2502);
  assert(mulib_imtui_cells(&ui)[20].codepoint == 0x2518);
  assert(mulib_imtui_cells(&ui)[27].codepoint == 0xfffd);
  assert(mulib_imtui_cells(&ui)[28].codepoint == 0xfffd);
  assert(mulib_imtui_cells(&ui)[29].codepoint == 'z');
  mulib_imtui_end(&ui);
  mulib_imtui_free(&ui);
}

static void
draw_frame(mulib_imtui_t *ui, int second)
{
  mulib_term_style_t style = mulib_term_style_default(), bold = style, rgb = style;
  bold.attributes = MULIB_TERM_BOLD;
  rgb.foreground = MULIB_TERM_RGB(35, 77, 119);
  assert(mulib_imtui_begin(ui, NULL, 0));
  assert(mulib_imtui_text(ui, 0, 0, "abcdef", 6, style));
  assert(mulib_imtui_text(ui, 1, 1, "\xc3\xa9!", 3, rgb));
  assert(mulib_imtui_text(ui, 0, 2, "bottom", 6, style));
  if (second)
    {
      assert(mulib_imtui_text(ui, 0, 0, "XY", 2, bold));
      assert(mulib_imtui_text(ui, 2, 0, "Z", 1, style));
      assert(mulib_imtui_text(ui, 4, 0, "e", 1, bold)); /* Style only. */
    }
  mulib_imtui_end(ui);
}

static unsigned
occurrences(const char *s, const char *pattern)
{
  unsigned n = 0;
  while ((s = strstr(s, pattern)) != NULL)
    {
      ++n;
      s += strlen(pattern);
    }
  return n;
}

static void
expect_canvas(const mulib_vterm_t *terminal, const mulib_imtui_t *ui)
{
  mulib_imtui_rect_t r = mulib_imtui_bounds(ui);
  const mulib_term_cell_t *cells = mulib_imtui_cells(ui);
  int x, y;
  for (y = 0; y < r.h; ++y)
    for (x = 0; x < r.w; ++x)
      assert(mulib_term_cell_equal(*cells++, *mulib_vterm_cell(terminal, (size_t)x, (size_t)y)));
}

static void
store16(unsigned char *p, unsigned n)
{
  p[0] = (unsigned char)n;
  p[1] = (unsigned char)(n >> 8);
}

/* Immediate automatic margins; optional missing rc causes a late failure.
 * The terminfo bytes are entirely synthetic and host-independent. */
static void
make_profile(mulib_terminfo_t *info, int restore)
{
  static const struct { unsigned index; const char *text; } caps[] = {
    {10, "\033[%i%p1%d;%p2%dH"}, {27, "\033[1m"}, {39, "\033[0m"},
    {126, "\033" "8"}, {128, "\033" "7"}, {151, "\033[?7h"}, {152, "\033[?7l"},
    {297, "\033[39;49m"}
  };
  unsigned char record[700] = {0};
  const size_t offsets = 16, table = offsets + 2 * 298;
  size_t pos = table, i;
  store16(record, 0x011a);
  store16(record + 2, 2);
  store16(record + 4, 2);
  store16(record + 8, 298);
  record[12] = 't';
  record[15] = 1; /* am */
  memset(record + offsets, 0xff, table - offsets);
  for (i = 0; i < sizeof(caps) / sizeof(caps[0]); ++i)
    {
      size_t n = strlen(caps[i].text) + 1;
      if (caps[i].index == 126 && !restore) continue;
      assert(pos + n <= sizeof(record));
      store16(record + offsets + 2 * caps[i].index, (unsigned)(pos - table));
      memcpy(record + pos, caps[i].text, n);
      pos += n;
    }
  store16(record + 10, (unsigned)(pos - table));
  assert(mulib_terminfo_load(info, record, pos));
}

static void
test_differential(void)
{
  mulib_imtui_t ui;
  mulib_imtui_renderer_t renderer;
  mulib_terminfo_t ansi, unsupported, same_ansi;
  mulib_vterm_t terminal;
  mulib_strbuf_t queued, snapshot, rejected;
  size_t first, second;
  const char *diff;
  unsigned flags = MULIB_TERMSTYLE_TRUECOLOR;
  assert(mulib_imtui_init(&ui, 6, 3));
  assert(mulib_vterm_init(&terminal, 6, 3));
  assert(mulib_vterm_move(&terminal, 4, 1));
  mulib_imtui_renderer_init(&renderer);
  mulib_terminfo_init(&ansi);
  mulib_terminfo_ansi(&ansi);
  mulib_terminfo_init(&same_ansi);
  mulib_terminfo_ansi(&same_ansi);
  mulib_terminfo_init(&unsupported);
  mulib_strbuf_init(&queued);
  mulib_strbuf_init(&snapshot);

  draw_frame(&ui, 0);
  assert(mulib_imtui_render(&renderer, &ui, &ansi, flags, &queued));
  first = queued.length;
  assert(first > 4);
  assert(!memcmp(queued.data, "\033" "7", 2));
  assert(!memcmp(queued.data + first - 2, "\033" "8", 2));
  assert(strstr(queued.data, "\033[38;2;35;77;119m"));
  assert(mulib_vterm_feed(&terminal, queued.data, first));
  expect_canvas(&terminal, &ui);
  assert(terminal.cursor_x == 4 && terminal.cursor_y == 1 && !terminal.wrap_pending);
  assert(mulib_imtui_render(&renderer, &ui, &same_ansi, flags, &queued));
  assert(queued.length == first);

  draw_frame(&ui, 1);
  assert(mulib_imtui_render(&renderer, &ui, NULL, MULIB_TERMSTYLE_PLAIN, &snapshot));
  assert(!strcmp(snapshot.data, "XYZdef\n \xc3\xa9!   \nbottom\n"));
  make_profile(&unsupported, 0);
  assert(!mulib_imtui_render(&renderer, &ui, &unsupported, flags, &queued));
  assert(!mulib_imtui_render(&renderer, &ui, &ansi, flags | 4u, &queued));
  rejected = queued;
  rejected.capacity = 0; /* Force only the final destination append to fail. */
  assert(!mulib_imtui_render(&renderer, &ui, &ansi, flags, &rejected));
  assert(queued.length == first && !memcmp(queued.data + first - 2, "\033" "8", 2));

  assert(mulib_imtui_render(&renderer, &ui, &ansi, flags, &queued));
  second = queued.length;
  diff = queued.data + first;
  assert(strstr(diff, "\033[1;1H") && strstr(diff, "\033[1;5H"));
  assert(occurrences(diff, "H") == 2);
  assert(occurrences(diff, "\033[1m") == 2 && strstr(diff, "XY"));
  assert(!strstr(diff, "bottom"));
  /* Replay the second queued frame byte-for-byte, using arbitrary chunks. */
  for (size_t i = first; i < second; ++i)
    assert(mulib_vterm_feed(&terminal, queued.data + i, 1));
  expect_canvas(&terminal, &ui);
  assert(terminal.cursor_x == 4 && terminal.cursor_y == 1);
  assert(mulib_imtui_render(&renderer, &ui, &ansi, flags, &queued));
  assert(queued.length == second);

  mulib_imtui_renderer_invalidate(&renderer);
  assert(mulib_imtui_render(&renderer, &ui, &ansi, flags, &queued));
  assert(strstr(queued.data + second, "bottom"));
  second = queued.length;
  assert(mulib_imtui_resize(&ui, 7, 4));
  assert(mulib_imtui_render(&renderer, &ui, &ansi, flags, &queued));
  assert(strstr(queued.data + second, "\033[4;1H"));
  second = queued.length;
  assert(mulib_imtui_render(&renderer, &ui, &ansi, 0, &queued));
  assert(strstr(queued.data + second, "\033[38;5;")); /* Repaint/quantize on flag change. */
  mulib_strbuf_free(&queued);
  mulib_strbuf_free(&snapshot);
  mulib_terminfo_free(&ansi);
  mulib_terminfo_free(&same_ansi);
  mulib_terminfo_free(&unsupported);
  mulib_imtui_renderer_free(&renderer);
  mulib_imtui_renderer_free(&renderer);
  mulib_vterm_free(&terminal);
  mulib_imtui_free(&ui);
}

static void
test_capability_failure(void)
{
  mulib_imtui_t ui;
  mulib_imtui_renderer_t renderer;
  mulib_terminfo_t info;
  mulib_strbuf_t out;
  mulib_term_style_t style = mulib_term_style_default();
  size_t first;
  assert(mulib_imtui_init(&ui, 3, 1));
  assert(mulib_imtui_begin(&ui, NULL, 0));
  assert(mulib_imtui_text(&ui, 0, 0, "abc", 3, style));
  mulib_imtui_end(&ui);
  mulib_imtui_renderer_init(&renderer);
  mulib_terminfo_init(&info);
  mulib_strbuf_init(&out);
  assert(mulib_strbuf_append(&out, "queued"));
  make_profile(&info, 0);
  assert(!mulib_imtui_render(&renderer, &ui, &info, 0, &out));
  assert(!strcmp(out.data, "queued"));
  make_profile(&info, 1); /* Reload into the same info object, then retry. */
  assert(mulib_imtui_render(&renderer, &ui, &info, 0, &out));
  assert(strstr(out.data, "\033[?7l") && strstr(out.data, "\033[?7h"));
  assert(strstr(out.data, "\033[1;1Habc"));
  first = out.length;
  make_profile(&info, 1); /* Equal records, not pointer identity. */
  assert(mulib_imtui_render(&renderer, &ui, &info, 0, &out) && out.length == first);
  make_profile(&info, 0); /* Profile changes must not use a stale baseline. */
  assert(!mulib_imtui_render(&renderer, &ui, &info, 0, &out) && out.length == first);
  make_profile(&info, 1);
  assert(mulib_imtui_render(&renderer, &ui, &info, 0, &out) && out.length == first);
  mulib_strbuf_free(&out);
  mulib_terminfo_free(&info);
  mulib_imtui_renderer_free(&renderer);
  mulib_imtui_free(&ui);
}

int
main(void)
{
  test_drawing();
  test_differential();
  test_capability_failure();
  return 0;
}
