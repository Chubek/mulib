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
#include <stdio.h>
int main (void)
{
  mulib_curses_t *screen = mulib_curses_create (40, 8, NULL);
  mulib_curses_window_t *window = mulib_curses_newwin (30, 5, 2, 1);
  mulib_strbuf_t output;
  assert (screen && window);
  mulib_strbuf_init (&output);
  assert (mulib_curses_box (window));
  assert (mulib_curses_move (window, 2, 2));
  assert (mulib_curses_addnstr (window, "Hello, curses!", 14));
  assert (mulib_curses_noutrefresh (screen, window, 0));
  assert (mulib_curses_render (screen, &output));
  printf ("Rendered %zu terminal bytes\n", output.length);
  mulib_strbuf_free (&output);
  mulib_curses_delwin (window);
  mulib_curses_destroy (screen);
  return 0;
}
