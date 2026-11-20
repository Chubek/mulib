#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define MULIB_IMPL_GRID
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_VTERM
#define MULIB_IMPL_IMTUI_CORE
#include "mulib-imtui-core.h"

static mulib_term_cell_t
cell (uint32_t cp)
{
  mulib_term_cell_t c;
  c.codepoint = cp;
  c.style = mulib_term_style_default ();
  return c;
}

static mulib_termkey_event_t
key (mulib_termkey_code_t code, uint32_t cp, unsigned modifiers)
{
  mulib_termkey_event_t e = { 0 };
  e.type = MULIB_TERMKEY_KEY;
  e.key = code;
  e.codepoint = cp;
  e.modifiers = modifiers;
  return e;
}

static mulib_termkey_event_t
mouse (mulib_termkey_mouse_action_t action, unsigned button, size_t x,
       size_t y)
{
  mulib_termkey_event_t e = { 0 };
  e.type = MULIB_TERMKEY_MOUSE;
  e.mouse_action = action;
  e.mouse_button = button;
  e.x = x;
  e.y = y;
  return e;
}

static mulib_termkey_event_t
paste (mulib_termkey_event_type_t type, unsigned char byte)
{
  mulib_termkey_event_t e = { 0 };
  e.type = type;
  if (type == MULIB_TERMKEY_PASTE_TEXT)
    {
      e.raw[0] = byte;
      e.length = 1;
    }
  return e;
}

static void
check_rect (mulib_imtui_rect_t r, int x, int y, int w, int h)
{
  assert (r.x == x && r.y == y && r.w == w && r.h == h);
}

static void
test_frame (void)
{
  mulib_imtui_t ui;
  mulib_term_cell_t star = cell ('*');
  mulib_term_cell_t bad = cell (27);
  const mulib_term_cell_t *pixels;
  star.style.foreground = MULIB_TERM_RGB (12, 34, 56);
  star.style.attributes = MULIB_TERM_BOLD | MULIB_TERM_UNDERLINE;
  assert (!mulib_imtui_init (NULL, 1, 1));
  assert (!mulib_imtui_init (&ui, 0, 1));
  assert (!mulib_imtui_init (&ui, -1, 1));
  assert (!mulib_imtui_init (&ui, INT_MAX, INT_MAX));
  assert (!mulib_imtui_cells (&ui) && !mulib_imtui_begin (&ui, NULL, 0));
  mulib_imtui_free (&ui);
  assert (mulib_imtui_init (&ui, 7, 5));
  check_rect (mulib_imtui_bounds (&ui), 0, 0, 7, 5);
  pixels = mulib_imtui_cells (&ui);
  for (size_t i = 0; i < 35; ++i)
    assert (mulib_term_cell_equal (pixels[i], cell (' ')));
  assert (!mulib_imtui_put (&ui, 0, 0, star));
  assert (!mulib_imtui_begin (&ui, NULL, 1));
  assert (mulib_imtui_begin (&ui, NULL, 0));
  assert (!mulib_imtui_resize (&ui, 7, 5));
  assert (!mulib_imtui_put (&ui, -1, 0, star));
  assert (!mulib_imtui_put (&ui, 0, INT_MIN, star));
  assert (!mulib_imtui_put (&ui, INT_MAX, INT_MAX, star));
  assert (!mulib_imtui_put (&ui, 7, 0, star));
  assert (!mulib_imtui_put (&ui, 0, 5, star));
  assert (!mulib_imtui_put (&ui, 0, 0, bad));
  bad = star;
  bad.style.background = UINT32_C (0x100);
  assert (!mulib_imtui_put (&ui, 0, 0, bad));
  assert (mulib_imtui_put (&ui, 0, 0, star));
  assert (mulib_imtui_put (&ui, 6, 4, star));
  assert (!mulib_imtui_begin (&ui, NULL, 0));
  assert (mulib_term_cell_equal (pixels[0], star));
  assert (mulib_term_cell_equal (pixels[34], star));

  assert (mulib_imtui_clip_push (&ui, (mulib_imtui_rect_t){ 3, 2, INT_MAX,
                                                            INT_MAX }));
  check_rect (mulib_imtui_clip (&ui), 3, 2, 4, 3);
  assert (mulib_imtui_clip_push (&ui, (mulib_imtui_rect_t){ -4, -4, 9, 8 }));
  check_rect (mulib_imtui_clip (&ui), 3, 2, 2, 2);
  assert (!mulib_imtui_put (&ui, 2, 2, star));
  assert (!mulib_imtui_put (&ui, 5, 2, star));
  assert (!mulib_imtui_put (&ui, 3, 4, star));
  assert (mulib_imtui_put (&ui, 3, 2, star));
  assert (mulib_imtui_clip_push (&ui, (mulib_imtui_rect_t){ INT_MIN, 0,
                                                            INT_MAX, 5 }));
  assert (!mulib_imtui_put (&ui, 3, 2, star));
  mulib_imtui_clip_pop (&ui);
  assert (mulib_imtui_clip_push (&ui, (mulib_imtui_rect_t){ 3, 2, INT_MIN, 1 }));
  assert (!mulib_imtui_put (&ui, 3, 2, star));
  mulib_imtui_clip_pop (&ui);
  for (size_t i = 2; i < MULIB_IMTUI_STACK_CAPACITY; ++i)
    assert (mulib_imtui_clip_push (&ui, mulib_imtui_bounds (&ui)));
  assert (!mulib_imtui_clip_push (&ui, mulib_imtui_bounds (&ui)));
  assert (mulib_imtui_put (&ui, 3, 2, star));
  mulib_imtui_end (&ui);
  check_rect (mulib_imtui_clip (&ui), 0, 0, 7, 5);
  mulib_imtui_clip_pop (&ui);
  mulib_imtui_end (&ui);
  assert (!mulib_imtui_resize (&ui, INT_MAX, INT_MAX));
  assert (mulib_imtui_cells (&ui) == pixels);
  assert (mulib_imtui_resize (&ui, 7, 5));
  assert (mulib_imtui_cells (&ui) == pixels);
  assert (mulib_imtui_resize (&ui, 8, 6));
  pixels = mulib_imtui_cells (&ui);
  assert (mulib_term_cell_equal (pixels[4 * 8 + 6], star));
  assert (mulib_term_cell_equal (pixels[4 * 8 + 7], cell (' ')));
  assert (mulib_imtui_resize (&ui, 1, 1));
  assert (mulib_term_cell_equal (mulib_imtui_cells (&ui)[0], star));
  assert (mulib_imtui_begin (&ui, NULL, 0));
  assert (mulib_term_cell_equal (mulib_imtui_cells (&ui)[0], cell (' ')));
  mulib_imtui_free (&ui);
  mulib_imtui_free (&ui);
  check_rect (mulib_imtui_bounds (&ui), 0, 0, 0, 0);
}

static void
test_ids (void)
{
  mulib_imtui_t ui;
  mulib_imtui_id_t root, scope, nested;
  mulib_imtui_rect_t r = { 0, 0, 1, 1 };
  assert (mulib_imtui_init (&ui, 2, 2));
  root = mulib_imtui_id (&ui, "label", 5);
  scope = mulib_imtui_id (&ui, "window", 6);
  assert (root && scope && scope != root);
  assert (mulib_imtui_id (&ui, "l\0x", 3) != mulib_imtui_id (&ui, "l", 1));
  assert (mulib_imtui_id (&ui, NULL, 0) == mulib_imtui_id (&ui, "", 0));
  assert (!mulib_imtui_id (&ui, NULL, 1));
  assert (!mulib_imtui_id_push (&ui, scope));
  assert (mulib_imtui_begin (&ui, NULL, 0));
  assert (!mulib_imtui_id_push (&ui, 0));
  assert (mulib_imtui_id_push (&ui, scope));
  nested = mulib_imtui_id (&ui, "label", 5);
  assert (nested && nested != root);
  for (size_t i = 1; i < MULIB_IMTUI_STACK_CAPACITY; ++i)
    assert (mulib_imtui_id_push (&ui, scope));
  assert (!mulib_imtui_id_push (&ui, scope));
  for (size_t i = 1; i < MULIB_IMTUI_STACK_CAPACITY; ++i)
    mulib_imtui_id_pop (&ui);
  assert (nested == mulib_imtui_id (&ui, "label", 5));
  mulib_imtui_id_pop (&ui);
  mulib_imtui_id_pop (&ui);
  assert (root == mulib_imtui_id (&ui, "label", 5));
  for (mulib_imtui_id_t id = 1; id <= MULIB_IMTUI_MAX_ITEMS; ++id)
    assert (!mulib_imtui_interact (&ui, id, r, 1));
  assert (!mulib_imtui_interact (&ui, MULIB_IMTUI_MAX_ITEMS + 1, r, 1));
  mulib_imtui_end (&ui);
  assert (ui.previous_count == MULIB_IMTUI_MAX_ITEMS);
  mulib_imtui_free (&ui);
}

static void
test_pointer (void)
{
  mulib_imtui_t ui;
  mulib_imtui_rect_t a = { 0, 0, 3, 1 }, b = { 2, 0, 4, 1 };
  mulib_termkey_event_t events[2];
  assert (mulib_imtui_init (&ui, 8, 3));
  events[0] = mouse (MULIB_TERMKEY_PRESS, 1, 2, 0);
  events[1] = mouse (MULIB_TERMKEY_RELEASE, 1, 2, 0);
  assert (mulib_imtui_begin (&ui, events, 2));
  assert (mulib_imtui_interact (&ui, 1, a, 1)
          == (MULIB_IMTUI_HOVERED | MULIB_IMTUI_FOCUSED
              | MULIB_IMTUI_PRESSED | MULIB_IMTUI_ACTIVATED));
  assert (!mulib_imtui_interact (&ui, 1, a, 1));
  assert (!mulib_imtui_interact (&ui, 2, b, 1));
  mulib_imtui_end (&ui);

  events[0] = mouse (MULIB_TERMKEY_PRESS, 1, 5, 0);
  events[1] = mouse (MULIB_TERMKEY_MOVE, 1, SIZE_MAX, SIZE_MAX);
  assert (mulib_imtui_begin (&ui, events, 2));
  assert (!mulib_imtui_interact (&ui, 1, a, 1));
  assert (mulib_imtui_interact (&ui, 2, b, 1)
          == (MULIB_IMTUI_FOCUSED | MULIB_IMTUI_PRESSED | MULIB_IMTUI_HELD));
  mulib_imtui_end (&ui);
  assert (mulib_imtui_begin (&ui, NULL, 0));
  assert (!mulib_imtui_interact (&ui, 1, a, 1));
  assert (mulib_imtui_interact (&ui, 2, b, 1)
          == (MULIB_IMTUI_FOCUSED | MULIB_IMTUI_HELD));
  mulib_imtui_end (&ui);
  events[0] = mouse (MULIB_TERMKEY_RELEASE, 0, 6, 0);
  assert (mulib_imtui_begin (&ui, events, 1));
  assert (mulib_imtui_interact (&ui, 2, b, 1) == MULIB_IMTUI_FOCUSED);
  mulib_imtui_end (&ui);

  events[0] = mouse (MULIB_TERMKEY_PRESS, 2, 7, 0);
  assert (mulib_imtui_begin (&ui, events, 1));
  assert (mulib_imtui_interact (&ui, 2, b, 1) == MULIB_IMTUI_FOCUSED);
  mulib_imtui_end (&ui);
  events[0] = mouse (MULIB_TERMKEY_PRESS, 1, 7, 0);
  events[1] = mouse (MULIB_TERMKEY_RELEASE, 1, 5, 0);
  assert (mulib_imtui_begin (&ui, events, 2));
  assert (mulib_imtui_interact (&ui, 2, b, 1) == MULIB_IMTUI_HOVERED);
  mulib_imtui_end (&ui);

  events[0] = mouse (MULIB_TERMKEY_PRESS, 1, 5, 0);
  assert (mulib_imtui_begin (&ui, events, 1));
  assert (mulib_imtui_interact (&ui, 2, b, 1) & MULIB_IMTUI_HELD);
  mulib_imtui_end (&ui);
  assert (mulib_imtui_resize (&ui, 4, 3));
  events[0] = mouse (MULIB_TERMKEY_RELEASE, 1, 3, 0);
  assert (mulib_imtui_begin (&ui, events, 1));
  assert (mulib_imtui_interact (&ui, 2, b, 1)
          == (MULIB_IMTUI_FOCUSED | MULIB_IMTUI_HOVERED));
  mulib_imtui_end (&ui);

  assert (mulib_imtui_resize (&ui, 2, 3));
  assert (!ui.focus && !ui.active);
  events[0] = mouse (MULIB_TERMKEY_PRESS, 1, 1, 0);
  assert (mulib_imtui_begin (&ui, events, 1));
  assert (mulib_imtui_interact (&ui, 1, a, 1) & MULIB_IMTUI_HELD);
  mulib_imtui_end (&ui);
  assert (mulib_imtui_begin (&ui, NULL, 0));
  assert (mulib_imtui_clip_push (&ui, (mulib_imtui_rect_t){ 0, 1, 2, 1 }));
  assert (!mulib_imtui_interact (&ui, 1, a, 1));
  mulib_imtui_end (&ui);
  assert (!ui.focus && !ui.active);
  events[0] = mouse (MULIB_TERMKEY_RELEASE, 1, 1, 0);
  assert (mulib_imtui_begin (&ui, events, 1));
  assert (mulib_imtui_interact (&ui, 1, a, 1) == MULIB_IMTUI_HOVERED);
  mulib_imtui_end (&ui);
  mulib_imtui_free (&ui);
}

static void
test_keys (void)
{
  mulib_imtui_t ui;
  mulib_imtui_rect_t a = { 0, 0, 1, 1 }, b = { 2, 0, 1, 1 };
  mulib_termkey_event_t e, events[9];
  assert (mulib_imtui_init (&ui, 5, 3));
  e = key (MULIB_TERMKEY_TAB, 0, 0);
  assert (!mulib_imtui_begin (&ui, &e, SIZE_MAX));
  assert (mulib_imtui_begin (&ui, NULL, 0));
  assert (!mulib_imtui_interact (&ui, 1, a, 1));
  assert (!mulib_imtui_interact (&ui, 3, a, 0));
  assert (!mulib_imtui_interact (&ui, 4, (mulib_imtui_rect_t){ 5, 0, 1, 1 }, 1));
  assert (!mulib_imtui_interact (&ui, 2, b, 1));
  mulib_imtui_end (&ui);
  events[0] = key (MULIB_TERMKEY_TAB, 0, 0);
  events[1] = key (MULIB_TERMKEY_CHAR, 'a', 0);
  events[2] = key (MULIB_TERMKEY_TAB, 0, 0);
  events[3] = key (MULIB_TERMKEY_ENTER, 0, 0);
  events[4] = key (MULIB_TERMKEY_CHAR, ' ', 0);
  events[5] = key (MULIB_TERMKEY_TAB, 0, MULIB_TERMKEY_CTRL);
  events[6] = key (MULIB_TERMKEY_TAB, 0, 0);
  events[7] = key (MULIB_TERMKEY_ENTER, 0, 0);
  events[8] = key (MULIB_TERMKEY_TAB, 0, MULIB_TERMKEY_SHIFT);
  assert (mulib_imtui_begin (&ui, events, 9));
  assert (!mulib_imtui_take_key (&ui, 1, &e));
  assert (mulib_imtui_interact_ex (&ui, 2, b, 1, 0) == MULIB_IMTUI_FOCUSED);
  assert (mulib_imtui_interact (&ui, 1, a, 1) == MULIB_IMTUI_ACTIVATED);
  assert (mulib_imtui_take_key (&ui, 1, &e) && e.codepoint == 'a');
  assert (!mulib_imtui_take_key (&ui, 1, &e));
  assert (mulib_imtui_take_key (&ui, 2, &e) && e.key == MULIB_TERMKEY_ENTER);
  assert (mulib_imtui_take_key (&ui, 2, &e) && e.codepoint == ' ');
  assert (mulib_imtui_take_key (&ui, 2, &e) && e.key == MULIB_TERMKEY_TAB
          && e.modifiers == MULIB_TERMKEY_CTRL);
  assert (!mulib_imtui_take_key (&ui, 2, &e));
  mulib_imtui_end (&ui);

  events[0] = key (MULIB_TERMKEY_TAB, 0, MULIB_TERMKEY_SHIFT);
  events[1] = key (MULIB_TERMKEY_CHAR, ' ', 0);
  assert (mulib_imtui_begin (&ui, events, 2));
  assert (!mulib_imtui_interact (&ui, 2, b, 1));
  assert (mulib_imtui_interact (&ui, 1, a, 1)
          == (MULIB_IMTUI_FOCUSED | MULIB_IMTUI_ACTIVATED));
  mulib_imtui_end (&ui);
  assert (!mulib_imtui_take_key (&ui, 1, &e));
  mulib_imtui_free (&ui);
}

static void
test_paste (void)
{
  mulib_imtui_t ui;
  mulib_imtui_rect_t a = { 0, 0, 1, 1 }, b = { 1, 0, 1, 1 };
  mulib_termkey_event_t e, events[8];
  assert (mulib_imtui_init (&ui, 2, 1));
  events[0] = mouse (MULIB_TERMKEY_PRESS, 1, 0, 0);
  events[1] = mouse (MULIB_TERMKEY_RELEASE, 1, 0, 0);
  events[2] = paste (MULIB_TERMKEY_PASTE_BEGIN, 0);
  events[3] = mouse (MULIB_TERMKEY_PRESS, 1, 1, 0);
  events[4] = mouse (MULIB_TERMKEY_RELEASE, 1, 1, 0);
  events[5] = key (MULIB_TERMKEY_CHAR, 'b', 0);
  events[6] = paste (MULIB_TERMKEY_PASTE_TEXT, 0xe2);
  assert (mulib_imtui_begin (&ui, events, 7));
  assert (!(mulib_imtui_interact_ex (&ui, 1, a, 1, 0) & MULIB_IMTUI_FOCUSED));
  assert (mulib_imtui_interact_ex (&ui, 2, b, 1, 0) & MULIB_IMTUI_FOCUSED);
  assert (mulib_imtui_take_key (&ui, 2, &e) && e.codepoint == 'b');
  assert (!mulib_imtui_take_key (&ui, 2, &e));
  assert (mulib_imtui_take_key (&ui, 1, &e) && e.type == MULIB_TERMKEY_PASTE_BEGIN);
  assert (mulib_imtui_take_key (&ui, 1, &e) && e.type == MULIB_TERMKEY_PASTE_TEXT
          && e.length == 1 && e.raw[0] == 0xe2);
  mulib_imtui_end (&ui);
  events[0] = paste (MULIB_TERMKEY_PASTE_TEXT, 0x82);
  events[1] = paste (MULIB_TERMKEY_PASTE_TEXT, 0xac);
  events[2] = paste (MULIB_TERMKEY_PASTE_END, 0);
  events[3] = key (MULIB_TERMKEY_CHAR, 'c', 0);
  assert (mulib_imtui_begin (&ui, events, 4));
  assert (!mulib_imtui_interact_ex (&ui, 1, a, 1, 0));
  assert (mulib_imtui_interact_ex (&ui, 2, b, 1, 0) & MULIB_IMTUI_FOCUSED);
  assert (mulib_imtui_take_key (&ui, 1, &e) && e.raw[0] == 0x82);
  assert (mulib_imtui_take_key (&ui, 1, &e) && e.raw[0] == 0xac);
  assert (mulib_imtui_take_key (&ui, 1, &e) && e.type == MULIB_TERMKEY_PASTE_END);
  assert (!mulib_imtui_take_key (&ui, 1, &e));
  assert (mulib_imtui_take_key (&ui, 2, &e) && e.codepoint == 'c');
  mulib_imtui_end (&ui);
  mulib_imtui_free (&ui);
}

static void
test_bootstrap_order (void)
{
  mulib_imtui_t ui;
  mulib_imtui_rect_t a = { 0, 0, 1, 1 };
  mulib_termkey_event_t e, events[8];
  assert (mulib_imtui_init (&ui, 1, 1));
  events[0] = mouse (MULIB_TERMKEY_PRESS, 1, 0, 0);
  events[1] = mouse (MULIB_TERMKEY_RELEASE, 1, 0, 0);
  events[2] = paste (MULIB_TERMKEY_PASTE_BEGIN, 0);
  events[3] = mouse (MULIB_TERMKEY_PRESS, 1, 0, 0);
  events[4] = mouse (MULIB_TERMKEY_RELEASE, 1, 0, 0);
  events[5] = key (MULIB_TERMKEY_CHAR, 'x', 0);
  events[6] = paste (MULIB_TERMKEY_PASTE_TEXT, 'y');
  events[7] = paste (MULIB_TERMKEY_PASTE_END, 0);
  assert (mulib_imtui_begin (&ui, events, 8));
  assert (mulib_imtui_interact_ex (&ui, 1, a, 1, 0)
          == (MULIB_IMTUI_HOVERED | MULIB_IMTUI_FOCUSED
              | MULIB_IMTUI_PRESSED | MULIB_IMTUI_ACTIVATED));
  for (size_t i = 2; i < 8; ++i)
    if (events[i].type != MULIB_TERMKEY_MOUSE)
      {
        assert (mulib_imtui_take_key (&ui, 1, &e));
        assert (e.type == events[i].type && e.codepoint == events[i].codepoint
                && e.raw[0] == events[i].raw[0]);
      }
  assert (!mulib_imtui_take_key (&ui, 1, &e));
  mulib_imtui_end (&ui);
  mulib_imtui_free (&ui);
}

int
main (void)
{
  test_frame ();
  test_ids ();
  test_pointer ();
  test_keys ();
  test_paste ();
  test_bootstrap_order ();
  puts ("imtui core tests passed");
  return 0;
}
