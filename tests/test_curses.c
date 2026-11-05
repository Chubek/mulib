#define MULIB_IMPL_CURSES
#define MULIB_IMPL_TUI
#define MULIB_IMPL_VTERM
#define MULIB_IMPL_GRID
#define MULIB_IMPL_TERMINFO
#define MULIB_IMPL_TERMKEY
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_TERMUTILS
#include "mulib-curses.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

int main (void)
{
  mulib_terminfo_t info;
  mulib_curses_t *screen;
  mulib_curses_window_t *base, *overlay;
  mulib_vterm_t oracle;
  mulib_strbuf_t bytes;
  mulib_termkey_event_t event;
  size_t x, y, before;
  mulib_terminfo_init (&info); mulib_terminfo_ansi (&info);
  screen = mulib_curses_create (8, 4, &info);
  mulib_terminfo_free (&info); /* Screen owns its capability selection. */
  base = mulib_curses_newwin (8, 4, 0, 0);
  overlay = mulib_curses_newwin (3, 2, 6, 1);
  assert (screen && base && overlay);
  assert (!mulib_curses_create (SIZE_MAX, 2, NULL));
  assert (!mulib_curses_newwin (0, 1, 0, 0));
  assert (mulib_curses_box (base));
  assert (mulib_curses_move (base, 1, 1));
  assert (mulib_curses_addnstr (base, "abc", 3));
  assert (!mulib_curses_addnstr (base, "x\033", 2));
  assert (mulib_curses_cell (base, 4, 1)->codepoint == ' ');
  assert (mulib_curses_addnstr (overlay, "123456", 6));
  assert (!mulib_curses_addnstr (overlay, "7", 1));
  assert (mulib_curses_cell (overlay, 2, 1)->codepoint == '6');
  mulib_curses_scrollok (overlay, 1);
  assert (mulib_curses_addnstr (overlay, "7", 1));
  assert (mulib_curses_cell (overlay, 0, 0)->codepoint == '4');
  assert (mulib_curses_cell (overlay, 0, 1)->codepoint == '7');
  assert (mulib_curses_noutrefresh (screen, base, 0));
  assert (mulib_curses_noutrefresh (screen, overlay, 1));
  mulib_strbuf_init (&bytes);
  assert (mulib_curses_render (screen, &bytes));
  assert (mulib_vterm_init (&oracle, 8, 4));
  assert (mulib_vterm_feed (&oracle, bytes.data, bytes.length));
  for (y = 0; y < 4; ++y) for (x = 0; x < 8; ++x)
    {
      const mulib_term_cell_t *expected = (x >= 6 && y >= 1 && y < 3)
          ? mulib_curses_cell (overlay, x - 6, y - 1) : mulib_curses_cell (base, x, y);
      assert (mulib_term_cell_equal (*expected, *mulib_vterm_cell (&oracle, x, y)));
    }
  before = bytes.length;
  assert (mulib_curses_render (screen, &bytes) && bytes.length == before);
  assert (mulib_curses_addnstr (overlay, "8", 1));
  assert (mulib_curses_noutrefresh (screen, overlay, 1));
  assert (!mulib_curses_refresh (screen, INT_MAX)); /* Failed write invalidates. */
  assert (mulib_curses_render (screen, &bytes) && bytes.length > before);
  assert (mulib_curses_feed (screen, "\033[", 2) == 2);
  assert (!mulib_curses_getkey (screen, &event, 0));
  assert (mulib_curses_feed (screen, "A", 1) == 1);
  assert (mulib_curses_getkey (screen, &event, 0) == 1 && event.key == MULIB_TERMKEY_UP);
  assert (mulib_curses_feed (screen, "\033", 1) == 1);
  assert (mulib_curses_getkey (screen, &event, 1) == 1 && event.key == MULIB_TERMKEY_ESCAPE);
  assert (!mulib_curses_resize (screen, 0, 3));
  assert (mulib_curses_resize (screen, 10, 5));
  assert (mulib_curses_mvwin (overlay, SIZE_MAX, SIZE_MAX));
  assert (mulib_curses_noutrefresh (screen, overlay, 0));
  assert (mulib_curses_scroll (overlay, INT_MIN));
  assert (mulib_curses_cell (overlay, 0, 0)->codepoint == ' ');
  assert (mulib_curses_modes (screen, 1, &bytes));
  assert (mulib_curses_modes (screen, 0, &bytes));
  mulib_curses_destroy (screen);
  mulib_terminfo_init (&info); /* Missing capabilities: atomic render failure. */
  screen = mulib_curses_create (8, 4, &info);
  assert (screen);
  assert (mulib_curses_noutrefresh (screen, base, 0));
  before = bytes.length;
  assert (!mulib_curses_render (screen, &bytes) && bytes.length == before);
  mulib_curses_destroy (screen);
  mulib_curses_delwin (base); mulib_curses_delwin (overlay);
  mulib_vterm_free (&oracle); mulib_strbuf_free (&bytes);
  puts ("curses: transactional windows, composition, terminal roundtrip and recovery passed");
  return 0;
}
