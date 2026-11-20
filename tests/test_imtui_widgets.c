#include <assert.h>
#include <limits.h>
#include <string.h>

#define MULIB_IMPL_UTF8
#define MULIB_IMPL_GRID
#define MULIB_IMPL_VTERM
#define MULIB_IMPL_TERMKEY
#define MULIB_IMPL_IMTUI_CORE
#define MULIB_IMPL_IMTUI_DRAW
#define MULIB_IMPL_IMTUI_WIDGETS
#include "mulib-imtui-widgets.h"

#define RECT(x, y, w, h) ((mulib_imtui_rect_t){ (x), (y), (w), (h) })

static void
begin (mulib_imtui_t *ui, mulib_termkey_event_t events[16], const char *input)
{
  mulib_termkey_t decoder;
  size_t count = 0, n = strlen (input);
  mulib_termkey_init (&decoder);
  assert (mulib_termkey_feed (&decoder, input, n) == n);
  while (decoder.length)
    {
      assert (count < 16);
      assert (mulib_termkey_next (&decoder, &events[count], 1) == 1);
      ++count;
    }
  assert (mulib_imtui_begin (ui, events, count));
}

static const mulib_term_cell_t *
cell (const mulib_imtui_t *ui, int x, int y)
{
  mulib_imtui_rect_t bounds = mulib_imtui_bounds (ui);
  assert (x >= 0 && x < bounds.w && y >= 0 && y < bounds.h);
  return &mulib_imtui_cells (ui)[(size_t)y * (size_t)bounds.w + (size_t)x];
}

static void
line (const mulib_imtui_t *ui, int x, int y, const char *text)
{
  for (; *text; ++x, ++text)
    assert (cell (ui, x, y)->codepoint == (unsigned char)*text);
}

static void
buttons_frame (mulib_imtui_t *ui, int *value, int button, int toggle)
{
  assert (mulib_imtui_button (ui, 1, RECT (0, 0, 8, 1), "Run", 3, 1)
          == button);
  assert (!mulib_imtui_button (ui, 9, RECT (0, 1, 8, 1), "Off", 3, 0));
  assert (mulib_imtui_checkbox (ui, 2, RECT (0, 2, 8, 1), "Flag", 4, value, 1)
          == toggle);
  line (ui, 0, 2, *value ? "[x] Flag" : "[ ] Flag");
  assert (cell (ui, 0, 1)->style.attributes == MULIB_TERM_DIM);
}

static void
test_activation (void)
{
  mulib_imtui_t ui;
  mulib_termkey_event_t events[16];
  int value = 0;
  assert (mulib_imtui_init (&ui, 12, 5));
  begin (&ui, events, "");
  buttons_frame (&ui, &value, 0, 0);
  line (&ui, 0, 0, "[ Run  ]");
  mulib_imtui_end (&ui);

  begin (&ui, events, "\t\r");
  buttons_frame (&ui, &value, 1, 0);
  assert (cell (&ui, 2, 0)->style.attributes & MULIB_TERM_UNDERLINE);
  mulib_imtui_end (&ui);

  begin (&ui, events, "\t ");
  buttons_frame (&ui, &value, 0, 1);
  assert (value == 1);
  assert (cell (&ui, 1, 2)->style.attributes & MULIB_TERM_UNDERLINE);
  mulib_imtui_end (&ui);

  /* A modified Space is not core activation; Shift-Tab skips disabled. */
  begin (&ui, events, "\033[32;5u\033[Z\r");
  buttons_frame (&ui, &value, 1, 0);
  assert (value == 1);
  mulib_imtui_end (&ui);

  /* Hover without focus, press/release separate frames, and cancel outside. */
  begin (&ui, events, "\033[<35;2;3M");
  buttons_frame (&ui, &value, 0, 0);
  assert (ui.hovered == 2);
  assert (cell (&ui, 0, 2)->style.attributes == MULIB_TERM_BOLD);
  mulib_imtui_end (&ui);
  begin (&ui, events, "\033[<0;2;3M");
  buttons_frame (&ui, &value, 0, 0);
  assert (cell (&ui, 0, 2)->style.attributes
          == (MULIB_TERM_BOLD | MULIB_TERM_INVERSE | MULIB_TERM_UNDERLINE));
  mulib_imtui_end (&ui);
  begin (&ui, events, "\033[<0;2;4m");
  buttons_frame (&ui, &value, 0, 0);
  mulib_imtui_end (&ui);
  begin (&ui, events, "\033[<0;2;3M\033[<0;2;3m");
  buttons_frame (&ui, &value, 0, 1);
  assert (value == 0);
  mulib_imtui_end (&ui);

  /* Click in the disabled row and try keyboard activation in the same frame. */
  begin (&ui, events, "\033[<0;2;2M\033[<0;2;2m\r");
  buttons_frame (&ui, &value, 0, 0);
  assert (ui.focus == 0 && value == 0);
  mulib_imtui_end (&ui);
  mulib_imtui_free (&ui);
}

static void
choices_frame (mulib_imtui_t *ui, int *selection, int change, int activation)
{
  assert (!mulib_imtui_radio (ui, 3, RECT (0, 0, 8, 1), "One", 3,
                             1, selection, 1, NULL));
  assert (mulib_imtui_radio (ui, 4, RECT (0, 1, 8, 1), "Two", 3,
                            2, selection, 1, NULL) == change);
  assert (mulib_imtui_selectable (ui, 5, RECT (0, 2, 8, 1), "List", 4,
                                 1, 1, NULL) == activation);
}

static void
test_choices (void)
{
  mulib_imtui_t ui;
  mulib_termkey_event_t events[16];
  int selection = 1;
  assert (mulib_imtui_init (&ui, 8, 3));
  begin (&ui, events, "");
  choices_frame (&ui, &selection, 0, 0);
  line (&ui, 0, 0, "(*) One");
  line (&ui, 0, 1, "( ) Two");
  line (&ui, 0, 2, "> List");
  mulib_imtui_end (&ui);

  begin (&ui, events, "\t\t ");
  choices_frame (&ui, &selection, 1, 0);
  assert (selection == 2);
  line (&ui, 0, 1, "(*) Two");
  mulib_imtui_end (&ui);
  /* Earlier options have already drawn when a later option changes;
   * the next frame displays the updated group state. */
  begin (&ui, events, "\r\t\r");
  choices_frame (&ui, &selection, 0, 1);
  line (&ui, 0, 0, "( ) One");
  assert (selection == 2);
  mulib_imtui_end (&ui);
  mulib_imtui_free (&ui);
}

static int
slider (mulib_imtui_t *ui, int *value, int enabled)
{
  return mulib_imtui_slider (ui, 1, RECT (2, 0, 5, 2), value, INT_MIN,
                             INT_MAX, enabled, NULL);
}

static void
test_slider (void)
{
  mulib_imtui_t ui;
  mulib_termkey_event_t events[16];
  int value = 0;
  assert (mulib_imtui_init (&ui, 10, 4));
  begin (&ui, events, "");
  assert (!slider (&ui, &value, 1));
  line (&ui, 2, 0, "==O--");
  assert (cell (&ui, 4, 1)->codepoint == '0');
  mulib_imtui_end (&ui);
  begin (&ui, events, "\t\033[C\033[A\033[D");
  assert (slider (&ui, &value, 1) && value == 1);
  mulib_imtui_end (&ui);
  begin (&ui, events, "\033[F\033[C");
  assert (slider (&ui, &value, 1) && value == INT_MAX);
  line (&ui, 2, 0, "====O");
  mulib_imtui_end (&ui);
  begin (&ui, events, "\033[H\033[B");
  assert (slider (&ui, &value, 1) && value == INT_MIN);
  line (&ui, 2, 0, "O----");
  mulib_imtui_end (&ui);
  begin (&ui, events, "\r \033[1;5C");
  assert (!slider (&ui, &value, 1) && value == INT_MIN);
  mulib_imtui_end (&ui);
  begin (&ui, events, "\033[<0;5;1M");
  assert (slider (&ui, &value, 1) && value == 0);
  mulib_imtui_end (&ui);
  begin (&ui, events, "\033[<32;9;1M");
  assert (slider (&ui, &value, 1) && value == INT_MAX);
  mulib_imtui_end (&ui);
  begin (&ui, events, "\033[<32;1;1M");
  assert (slider (&ui, &value, 1) && value == INT_MIN);
  mulib_imtui_end (&ui);
  begin (&ui, events, "\033[<0;7;1m");
  assert (slider (&ui, &value, 1) && value == INT_MAX);
  mulib_imtui_end (&ui);
  begin (&ui, events, "\033[H\033[<0;3;1M\033[<0;3;1m");
  assert (!slider (&ui, &value, 0) && value == INT_MAX);
  assert (cell (&ui, 2, 0)->style.attributes == MULIB_TERM_DIM);
  mulib_imtui_end (&ui);
  /* One column can take focus by mouse but can only be adjusted by keys. */
  begin (&ui, events, "");
  value = 1;
  assert (!mulib_imtui_slider (&ui, 2, RECT (0, 0, 1, 1), &value, 0, 2,
                               1, NULL));
  mulib_imtui_end (&ui);
  begin (&ui, events, "\033[<0;1;1M\033[<0;1;1m");
  assert (!mulib_imtui_slider (&ui, 2, RECT (0, 0, 1, 1), &value, 0, 2,
                               1, NULL));
  assert (value == 1 && ui.focus == 2);
  mulib_imtui_end (&ui);
  begin (&ui, events, "\033[F");
  assert (mulib_imtui_slider (&ui, 2, RECT (0, 0, 1, 1), &value, 0, 2,
                              1, NULL));
  assert (value == 2);
  mulib_imtui_end (&ui);
  mulib_imtui_free (&ui);
}

static void
test_geometry_theme (void)
{
  mulib_imtui_t ui;
  mulib_termkey_event_t events[16];
  mulib_imtui_theme_t theme = mulib_imtui_theme_default ();
  int value = 123;
  assert (mulib_imtui_init (&ui, 12, 8));
  begin (&ui, events, "");
  theme.normal.foreground = 6;
  theme.separator.background = 2;
  theme.progress_full.foreground = 10;
  theme.unicode = 1;
  mulib_imtui_label (&ui, RECT (0, 0, 5, 1), "A\xc3\xa9" "BCDE\nZ", 9,
                     &theme);
  assert (cell (&ui, 1, 0)->codepoint == 0xe9);
  assert (cell (&ui, 4, 0)->codepoint == 'D');
  assert (cell (&ui, 4, 0)->style.foreground == 6);
  assert (cell (&ui, 5, 0)->codepoint == ' ');
  mulib_imtui_label (&ui, RECT (7, 0, 5, 2), "a\rbc", 4, NULL);
  line (&ui, 7, 0, "a    ");
  assert (!mulib_imtui_button_themed (&ui, 1, RECT (0, 1, 5, 3), "12345",
                                     5, 1, &theme));
  assert (cell (&ui, 0, 1)->codepoint == 0x250c);
  assert (cell (&ui, 0, 2)->codepoint == 0x2502);
  assert (cell (&ui, 4, 2)->codepoint == 0x2502);
  line (&ui, 1, 2, "123");
  assert (!mulib_imtui_checkbox (&ui, 2, RECT (6, 1, 3, 1), NULL, 0, NULL, 1));
  line (&ui, 6, 1, "[ ]");
  assert (cell (&ui, 7, 1)->style.attributes == MULIB_TERM_DIM);
  assert (!mulib_imtui_checkbox (&ui, 3, RECT (6, 2, 1, 1), "x", 1, &value, 1));
  line (&ui, 6, 2, "x");

  assert (mulib_imtui_clip_push (&ui, RECT (2, 4, 5, 2)));
  mulib_imtui_progress (&ui, RECT (0, 4, 10, 1), 1, 2, &theme);
  line (&ui, 0, 4, "  ###.. ");
  assert (cell (&ui, 2, 4)->style.foreground == 10);
  mulib_imtui_label (&ui, RECT (-2, 5, 7, 1), "ABCDEFGH", 8, NULL);
  line (&ui, 0, 5, "  EFG ");
  mulib_imtui_clip_pop (&ui);
  mulib_imtui_separator (&ui, RECT (11, 0, 2, 3), 1, &theme);
  assert (cell (&ui, 11, 0)->codepoint == 0x2502);
  assert (cell (&ui, 11, 2)->style.background == 2);
  mulib_imtui_separator (&ui, RECT (7, 6, INT_MAX, 2), 0, NULL);
  line (&ui, 7, 6, "-----");
  assert (cell (&ui, 7, 7)->codepoint == ' ');
  mulib_imtui_progress (&ui, RECT (0, 6, 4, 1), UINT_MAX, 0, NULL);
  mulib_imtui_progress (&ui, RECT (0, 7, 4, 1), UINT_MAX, UINT_MAX - 1, NULL);
  line (&ui, 0, 6, "....");
  line (&ui, 0, 7, "####");
  assert (!mulib_imtui_slider (&ui, 4, RECT (5, 7, 1, 1), &value, 0,
                               2, 1, NULL));
  assert (value == 123 && cell (&ui, 5, 7)->codepoint == 'O');
  assert (!mulib_imtui_button (&ui, 5, RECT (INT_MIN, 0, INT_MAX, 1),
                              "offscreen", 9, 1));
  assert (!mulib_imtui_button (&ui, 6, RECT (INT_MAX, INT_MAX, INT_MAX, INT_MAX),
                              "offscreen", 9, 1));
  assert (!mulib_imtui_button (&ui, 7, RECT (0, 0, -1, 1), "empty", 5, 1));
  mulib_imtui_progress (&ui, RECT (INT_MAX - 1, INT_MAX - 1, INT_MAX, INT_MAX),
                        UINT_MAX - 1, UINT_MAX, NULL);
  mulib_imtui_end (&ui);
  mulib_imtui_free (&ui);
}

static void
test_clip_input (void)
{
  mulib_imtui_t ui;
  mulib_termkey_event_t events[16];
  int value = 0;
  assert (mulib_imtui_init (&ui, 8, 2));
  begin (&ui, events, "");
  for (unsigned i = 0; i < MULIB_IMTUI_STACK_CAPACITY; ++i)
    assert (mulib_imtui_clip_push (&ui, RECT (1, 0, 5, 1)));
  assert (!mulib_imtui_checkbox (&ui, 1, RECT (0, 0, 8, 1), "Test", 4,
                                &value, 1));
  line (&ui, 0, 0, "  ] Te  ");
  mulib_imtui_end (&ui);
  begin (&ui, events, "\033[<0;1;1M\033[<0;1;1m");
  assert (mulib_imtui_clip_push (&ui, RECT (1, 0, 5, 1)));
  assert (!mulib_imtui_checkbox (&ui, 1, RECT (0, 0, 8, 1), "Test", 4,
                                &value, 1));
  assert (value == 0);
  mulib_imtui_end (&ui);
  begin (&ui, events, "\033[<0;2;1M\033[<0;2;1m");
  assert (mulib_imtui_clip_push (&ui, RECT (1, 0, 5, 1)));
  assert (mulib_imtui_checkbox (&ui, 1, RECT (0, 0, 8, 1), "Test", 4,
                               &value, 1));
  line (&ui, 0, 0, " x] Te  ");
  assert (value == 1);
  mulib_imtui_end (&ui);
  mulib_imtui_free (&ui);
}

int
main (void)
{
  test_activation ();
  test_choices ();
  test_slider ();
  test_geometry_theme ();
  test_clip_input ();
  return 0;
}
