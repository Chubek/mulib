/** @file mulib-curses.h
 * @brief Retained curses-style windows, staged composition and differential refresh.
 * Define MULIB_IMPL_CURSES plus TUI, VTERM, GRID, TERMINFO, TERMKEY, UTF8,
 * STRBUF and TERMUTILS in one TU. Coordinates are zero-based (x,y).
 * Each Unicode scalar occupies one cell, as in vterm. No global screen or
 * implicit terminal mode changes. See manual/mulib-curses.md. */
#ifndef MULIB_CURSES_H
#define MULIB_CURSES_H
#include "mulib-tui.h"
#include "mulib-termutils.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_curses mulib_curses_t;
typedef struct mulib_curses_window mulib_curses_window_t;
/** @brief Allocate a screen and copy capabilities; NULL info selects ANSI. */
mulib_curses_t *mulib_curses_create (size_t width, size_t height, const mulib_terminfo_t *info);
/** @brief Free screen; independently owned windows remain valid. NULL allowed. */
void mulib_curses_destroy (mulib_curses_t *screen);
/** @brief Resize screen atomically, preserving overlap and invalidating display. */
int mulib_curses_resize (mulib_curses_t *, size_t width, size_t height);
/** @brief Create an independent blank window at screen origin x,y. */
mulib_curses_window_t *mulib_curses_newwin (size_t width, size_t height, size_t x, size_t y);
/** @brief Release a window; NULL allowed. */
void mulib_curses_delwin (mulib_curses_window_t *);
/** @brief Move the window's screen origin; composition clips to the screen. */
int mulib_curses_mvwin (mulib_curses_window_t *, size_t x, size_t y);
/** @brief Move the window-local insertion cursor in bounds. */
int mulib_curses_move (mulib_curses_window_t *, size_t x, size_t y);
/** @brief Set validated rendition for subsequent writes/erasure. */
int mulib_curses_style (mulib_curses_window_t *, mulib_term_style_t);
/** @brief Enable automatic scrolling on writes at the bottom margin. */
void mulib_curses_scrollok (mulib_curses_window_t *, int enabled);
/** @brief Write UTF-8 transactionally. LF/CR/TAB supported; other controls
 * rejected. Wraps at the right edge; non-scrolling overflow fails atomically. */
int mulib_curses_addnstr (mulib_curses_window_t *, const char *text, size_t length);
/** @brief Erase window in current style and home cursor. */
int mulib_curses_erase (mulib_curses_window_t *);
/** @brief Clear from cursor through end of current row. */
int mulib_curses_clrtoeol (mulib_curses_window_t *);
/** @brief Scroll window up (positive) or down, filling with current style. */
int mulib_curses_scroll (mulib_curses_window_t *, int lines);
/** @brief Draw ASCII border, preserving insertion cursor. */
int mulib_curses_box (mulib_curses_window_t *);
/** @brief Borrow a window cell until mutation/destruction. */
const mulib_term_cell_t *mulib_curses_cell (const mulib_curses_window_t *, size_t x, size_t y);
/** @brief Stage clipped opaque window contents. Later stages overwrite earlier
 * ones. A visible in-bounds window cursor becomes the screen cursor. */
int mulib_curses_noutrefresh (mulib_curses_t *, const mulib_curses_window_t *, int cursor_visible);
/** @brief Append differential output atomically. After success deliver all bytes;
 * invalidate on transport failure. Returns 1/0. */
int mulib_curses_render (mulib_curses_t *, mulib_strbuf_t *output);
/** @brief Render/write all bytes to fd; automatically invalidate on I/O failure. */
int mulib_curses_refresh (mulib_curses_t *, int fd);
/** @brief Force repaint after external output or a dropped render buffer. */
void mulib_curses_invalidate (mulib_curses_t *);
/** @brief Append optional alternate-screen/keypad entry or exit, atomically.
 * Caller owns raw mode and delivery; exit also restores rendition/cursor. */
int mulib_curses_modes (mulib_curses_t *, int enter, mulib_strbuf_t *output);
/** @brief Feed input with backpressure; pull events before retrying remainder. */
size_t mulib_curses_feed (mulib_curses_t *, const void *, size_t length);
/** @brief Pull termkey event; flush at Escape timeout/EOF. Returns 1/0/-1. */
int mulib_curses_getkey (mulib_curses_t *, mulib_termkey_event_t *, int flush);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CURSES) && !defined(MULIB_CURSES_IMPLEMENTED)
#define MULIB_CURSES_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_curses {
  mulib_tui_t ui;
  mulib_terminfo_t info;
  mulib_termkey_t keys;
};
struct mulib_curses_window {
  mulib_vterm_t cells;
  size_t x, y;
  int scrolling;
};
mulib_curses_t *mulib_curses_create (size_t w, size_t h, const mulib_terminfo_t *t)
{
  mulib_curses_t *s = (mulib_curses_t *)calloc (1, sizeof (*s));
  if (!s) return NULL;
  if (!mulib_tui_init (&s->ui, w, h)
      || (t && !mulib_terminfo_copy (&s->info, t)))
    { mulib_curses_destroy (s); return NULL; }
  if (!t) mulib_terminfo_ansi (&s->info);
  mulib_termkey_init (&s->keys);
  return s;
}
void mulib_curses_destroy (mulib_curses_t *s)
{
  if (!s) return;
  mulib_tui_free (&s->ui); mulib_terminfo_free (&s->info); free (s);
}
int mulib_curses_resize (mulib_curses_t *s, size_t w, size_t h)
{ return s && mulib_tui_resize (&s->ui, w, h); }
mulib_curses_window_t *mulib_curses_newwin (size_t w, size_t h, size_t x, size_t y)
{
  mulib_curses_window_t *v = (mulib_curses_window_t *)calloc (1, sizeof (*v));
  if (!v) return NULL;
  if (!mulib_vterm_init (&v->cells, w, h)) { free (v); return NULL; }
  v->x = x; v->y = y;
  return v;
}
void mulib_curses_delwin (mulib_curses_window_t *v)
{ if (v) { mulib_vterm_free (&v->cells); free (v); } }
int mulib_curses_mvwin (mulib_curses_window_t *v, size_t x, size_t y)
{ if (!v) return 0; v->x = x; v->y = y; return 1; }
int mulib_curses_move (mulib_curses_window_t *v, size_t x, size_t y)
{ return v && mulib_vterm_move (&v->cells, x, y); }
int mulib_curses_style (mulib_curses_window_t *v, mulib_term_style_t style)
{
  mulib_term_cell_t c;
  c.codepoint = ' '; c.style = style;
  if (!v || !mulib_term_cell_valid (c)) return 0;
  v->cells.style = style; return 1;
}
void mulib_curses_scrollok (mulib_curses_window_t *v, int enabled)
{ if (v) v->scrolling = !!enabled; }
static int mulib_curses_newline_ (mulib_vterm_t *v, int scroll)
{
  if (v->cursor_y + 1 == v->grid.height)
    { if (!scroll) return 0; mulib_vterm_scroll (v, 1); }
  else ++v->cursor_y;
  v->cursor_x = 0; v->wrap_pending = 0; return 1;
}
int mulib_curses_addnstr (mulib_curses_window_t *win, const char *text, size_t n)
{
  mulib_vterm_t v;
  size_t at = 0, used, count;
  uint32_t cp;
  mulib_term_cell_t cell;
  if (!win || (!text && n) || !mulib_utf8_validate ((const unsigned char *)text, n)) return 0;
  if (!mulib_vterm_init (&v, win->cells.grid.width, win->cells.grid.height)) return 0;
  memcpy (v.grid.data, win->cells.grid.data,
          v.grid.width * v.grid.height * sizeof (cell));
  v.style = win->cells.style; v.cursor_x = win->cells.cursor_x;
  v.cursor_y = win->cells.cursor_y; v.wrap_pending = win->cells.wrap_pending;
  cell.style = v.style;
  while (at < n)
    {
      if (!mulib_utf8_decode ((const unsigned char *)text + at, n - at, &cp, &used)) goto fail;
      at += used;
      if (cp == '\r') { v.cursor_x = 0; v.wrap_pending = 0; continue; }
      if (cp == '\n') { if (!mulib_curses_newline_ (&v, win->scrolling)) goto fail; continue; }
      if (v.wrap_pending && !mulib_curses_newline_ (&v, win->scrolling)) goto fail;
      count = cp == '\t' ? 8 - v.cursor_x % 8 : 1;
      cell.codepoint = cp == '\t' ? ' ' : cp;
      if (!mulib_term_cell_valid (cell)) goto fail;
      while (count--)
        {
          if (v.wrap_pending && !mulib_curses_newline_ (&v, win->scrolling)) goto fail;
          if (!mulib_vterm_set_cell (&v, v.cursor_x, v.cursor_y, cell)) goto fail;
          if (v.cursor_x + 1 == v.grid.width) v.wrap_pending = 1;
          else ++v.cursor_x;
        }
    }
  mulib_vterm_free (&win->cells); win->cells = v; return 1;
fail:
  mulib_vterm_free (&v); return 0;
}
int mulib_curses_erase (mulib_curses_window_t *v)
{
  mulib_term_cell_t c;
  if (!v) return 0;
  c.codepoint = ' '; c.style = v->cells.style;
  mulib_vterm_fill (&v->cells, 0, 0, v->cells.grid.width, v->cells.grid.height, c);
  return mulib_vterm_move (&v->cells, 0, 0);
}
int mulib_curses_clrtoeol (mulib_curses_window_t *v)
{
  mulib_term_cell_t c;
  if (!v) return 0;
  c.codepoint = ' '; c.style = v->cells.style;
  return mulib_vterm_fill (&v->cells, v->cells.cursor_x, v->cells.cursor_y,
                          v->cells.grid.width - v->cells.cursor_x, 1, c);
}
int mulib_curses_scroll (mulib_curses_window_t *v, int n)
{ if (!v) return 0; mulib_vterm_scroll (&v->cells, n); v->cells.wrap_pending = 0; return 1; }
int mulib_curses_box (mulib_curses_window_t *v)
{
  size_t x, y, w, h;
  mulib_term_cell_t c;
  if (!v) return 0;
  w = v->cells.grid.width; h = v->cells.grid.height; c.style = v->cells.style;
  for (y = 0; y < h; ++y) for (x = 0; x < w; ++x)
    if (!x || !y || x == w - 1 || y == h - 1)
      {
        c.codepoint = (!x || x == w - 1) ? ((!y || y == h - 1) ? '+' : '|') : '-';
        mulib_vterm_set_cell (&v->cells, x, y, c);
      }
  return 1;
}
const mulib_term_cell_t *mulib_curses_cell (const mulib_curses_window_t *v, size_t x, size_t y)
{ return v ? mulib_vterm_cell (&v->cells, x, y) : NULL; }
int mulib_curses_noutrefresh (mulib_curses_t *s, const mulib_curses_window_t *v, int visible)
{
  size_t x, y, w, h;
  if (!s || !v) return 0;
  w = s->ui.back.grid.width; h = s->ui.back.grid.height;
  if (v->x >= w || v->y >= h) return 1;
  w -= v->x; h -= v->y;
  if (w > v->cells.grid.width) w = v->cells.grid.width;
  if (h > v->cells.grid.height) h = v->cells.grid.height;
  for (y = 0; y < h; ++y) for (x = 0; x < w; ++x)
    mulib_tui_put (&s->ui, v->x + x, v->y + y, *mulib_vterm_cell (&v->cells, x, y));
  if (v->cells.cursor_x < w && v->cells.cursor_y < h)
    mulib_tui_cursor (&s->ui, v->x + v->cells.cursor_x, v->y + v->cells.cursor_y, visible);
  else s->ui.cursor_visible = 0;
  return 1;
}
int mulib_curses_render (mulib_curses_t *s, mulib_strbuf_t *out)
{ return s && mulib_tui_render (&s->ui, &s->info, out); }
void mulib_curses_invalidate (mulib_curses_t *s)
{ if (s) mulib_tui_invalidate (&s->ui); }
int mulib_curses_refresh (mulib_curses_t *s, int fd)
{
  mulib_strbuf_t out;
  size_t written;
  int ok;
  if (!s || fd < 0) return 0;
  mulib_strbuf_init (&out);
  ok = mulib_curses_render (s, &out);
  if (ok && mulib_termutils_write (fd, out.data, out.length, &written))
    { mulib_curses_invalidate (s); ok = 0; }
  mulib_strbuf_free (&out); return ok;
}
int mulib_curses_modes (mulib_curses_t *s, int enter, mulib_strbuf_t *out)
{
  static const char *const start[] = { "smcup", "smkx" };
  static const char *const stop[] = { "sgr0", "op", "cnorm", "rmkx", "rmcup" };
  const char *const *names = enter ? start : stop;
  size_t i, count = enter ? 2 : 5;
  mulib_strbuf_t temp;
  int ok = 1;
  if (!s || !out) return 0;
  mulib_strbuf_init (&temp);
  for (i = 0; i < count && ok; ++i)
    if (mulib_terminfo_str (&s->info, names[i]))
      ok = mulib_terminfo_emit (&s->info, names[i], NULL, &temp);
  if (ok) ok = mulib_strbuf_append_n (out, temp.data, temp.length);
  mulib_strbuf_free (&temp);
  if (ok) mulib_curses_invalidate (s);
  return ok;
}
size_t mulib_curses_feed (mulib_curses_t *s, const void *p, size_t n)
{ return s ? mulib_termkey_feed (&s->keys, p, n) : 0; }
int mulib_curses_getkey (mulib_curses_t *s, mulib_termkey_event_t *e, int flush)
{ return s ? mulib_termkey_next (&s->keys, e, flush) : -1; }
#ifdef __cplusplus
}
#endif
#endif
