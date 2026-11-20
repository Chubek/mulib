#define MULIB_IMPL_GRID
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_VTERM
#define MULIB_IMPL_IMTUI_CORE
#define MULIB_IMPL_IMTUI_LAYOUT
#define MULIB_IMPL_IMTUI_SCROLL
#include "mulib-imtui-layout.h"
#include "mulib-imtui-scroll.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>

static void
expect_rect(mulib_imtui_rect_t r, int x, int y, int w, int h)
{
  assert(r.x == x && r.y == y && r.w == w && r.h == h);
}

static void
equal_tracks(void)
{
  mulib_imtui_rect_t r, area = {-2, 5, 14, 8};
  assert(mulib_imtui_layout_row(area, 1, 3, 0, &r));
  expect_rect(r, -2, 5, 4, 8);
  assert(mulib_imtui_layout_row(area, 1, 3, 2, &r));
  expect_rect(r, 8, 5, 4, 8);
  assert(mulib_imtui_layout_grid(area, 2, 1, 3, 2, 1, 1, &r));
  expect_rect(r, 4, 10, 3, 3);
  assert(mulib_imtui_layout_grid(area, 2, 1, 3, 2, 2, 0, &r));
  expect_rect(r, 9, 5, 3, 4);

  for (int extent = 0; extent <= 9; ++extent)
    for (int count = 1; count <= 12; ++count)
      for (int gap = 0; gap <= 10; ++gap) {
        int actual_gap = count == 1 ? 0 : gap;
        int offset = 0, usable;
        mulib_imtui_rect_t a = {-6, -3, extent, extent};
        while (actual_gap * (count - 1) > extent)
          --actual_gap;
        usable = extent - actual_gap * (count - 1);
        for (int i = 0; i < count; ++i) {
          int width = usable / count + (i < usable % count);
          assert(mulib_imtui_layout_row(a, gap, (size_t)count, (size_t)i, &r));
          expect_rect(r, a.x + offset, a.y, width, extent);
          assert(mulib_imtui_layout_column(a, gap, (size_t)count, (size_t)i, &r));
          expect_rect(r, a.x, a.y + offset, extent, width);
          offset += width + (i + 1 < count ? actual_gap : 0);
        }
        assert(offset == extent);
      }
}

static void
equal_limits(void)
{
  mulib_imtui_rect_t r, area = {INT_MIN, INT_MAX - 3, 3, 3};
  assert(mulib_imtui_layout_grid(area, INT_MAX, INT_MAX, SIZE_MAX, SIZE_MAX,
                                 SIZE_MAX - 1, SIZE_MAX - 1, &r));
  expect_rect(r, INT_MIN + 3, INT_MAX, 0, 0);
  assert(mulib_imtui_layout_column(area, INT_MAX, 1, 0, &r));
  expect_rect(r, INT_MIN, INT_MAX - 3, 3, 3);
  assert(mulib_imtui_layout_row((mulib_imtui_rect_t){0, 0, INT_MAX, 1},
                              INT_MAX, 2, 1, &r));
  expect_rect(r, INT_MAX, 0, 0, 1);

  assert(!mulib_imtui_layout_row(area, 0, 0, 0, &r));
  assert(!mulib_imtui_layout_row(area, -1, 1, 0, &r));
  assert(!mulib_imtui_layout_column(area, 0, 1, 1, &r));
  assert(!mulib_imtui_layout_grid(area, 0, 0, 1, 1, 0, 1, &r));
  assert(!mulib_imtui_layout_grid(area, 0, 0, 1, 1, 0, 0, NULL));
  assert(!mulib_imtui_layout_row((mulib_imtui_rect_t){INT_MAX, 0, 1, 0},
                               0, 1, 0, &r));
  assert(!mulib_imtui_layout_column((mulib_imtui_rect_t){0, INT_MAX, 0, 1},
                                  0, 1, 0, &r));
  assert(!mulib_imtui_layout_column((mulib_imtui_rect_t){0, 0, -1, 1},
                                  0, 1, 0, &r));
  expect_rect(r, INT_MAX, 0, 0, 1);
}

static void
weighted_tracks(void)
{
  const unsigned weights[] = {1, 0, 2, 1};
  const unsigned zeros[] = {0, 0};
  const unsigned large[] = {UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX};
  mulib_imtui_rect_t r[4], area = {2, -3, 10, 16};

  assert(mulib_imtui_layout_weighted(area, MULIB_IMTUI_LAYOUT_Y, 1, weights, 4, r));
  expect_rect(r[0], 2, -3, 10, 3);
  expect_rect(r[1], 2, 1, 10, 0);
  expect_rect(r[2], 2, 2, 10, 6);
  expect_rect(r[3], 2, 9, 10, 4);

  assert(mulib_imtui_layout_weighted(area, MULIB_IMTUI_LAYOUT_X, INT_MAX,
                                    weights, 4, r));
  expect_rect(r[0], 2, -3, 0, 16);
  expect_rect(r[1], 5, -3, 0, 16);
  expect_rect(r[2], 8, -3, 0, 16);
  expect_rect(r[3], 11, -3, 1, 16);
  assert(!mulib_imtui_layout_weighted(area, MULIB_IMTUI_LAYOUT_Y, 0, zeros, 2, r));
  assert(!mulib_imtui_layout_weighted(area, MULIB_IMTUI_LAYOUT_Y, 0, weights, 0, r));
  assert(!mulib_imtui_layout_weighted(area, MULIB_IMTUI_LAYOUT_Y, 0, weights,
                                     SIZE_MAX, r));
  assert(!mulib_imtui_layout_weighted(area, (mulib_imtui_layout_axis_t)2,
                                     0, weights, 4, r));
  assert(!mulib_imtui_layout_weighted(area, MULIB_IMTUI_LAYOUT_X, -1,
                                     weights, 4, r));
  expect_rect(r[0], 2, -3, 0, 16);
  expect_rect(r[1], 5, -3, 0, 16);
  expect_rect(r[2], 8, -3, 0, 16);
  expect_rect(r[3], 11, -3, 1, 16);

  area = (mulib_imtui_rect_t){0, INT_MIN, INT_MAX, 7};
  if (UINTMAX_MAX / 4 >= UINT_MAX) {
    int before = 0;
    assert(mulib_imtui_layout_weighted(area, MULIB_IMTUI_LAYOUT_X, 0, large, 4, r));
    for (int i = 1; i <= 4; ++i) {
      int after = (INT_MAX / 4) * i + ((INT_MAX % 4) * i) / 4;
      expect_rect(r[i - 1], before, INT_MIN, after - before, 7);
      before = after;
    }
    assert(before == INT_MAX);
  } else {
    assert(!mulib_imtui_layout_weighted(area, MULIB_IMTUI_LAYOUT_X, 0, large, 4, r));
    expect_rect(r[3], 11, -3, 1, 16);
  }
  area = (mulib_imtui_rect_t){0, INT_MIN, 0, 7};
  assert(mulib_imtui_layout_weighted(area, MULIB_IMTUI_LAYOUT_X, INT_MAX,
                                    weights, 4, r));
  for (size_t i = 0; i < 4; ++i)
    expect_rect(r[i], 0, INT_MIN, 0, 7);
}

static void
padding_alignment(void)
{
  mulib_imtui_rect_t r, area = {-10, 5, 9, 6};
  assert(mulib_imtui_layout_pad(area, 1, 2, 3, 0, &r));
  expect_rect(r, -9, 7, 5, 4);
  assert(mulib_imtui_layout_pad(area, INT_MAX, 1, INT_MAX, INT_MAX, &r));
  expect_rect(r, -1, 6, 0, 0);
  assert(!mulib_imtui_layout_pad(area, 0, -1, 0, 0, &r));
  expect_rect(r, -1, 6, 0, 0);

  assert(mulib_imtui_layout_align(area, 4, 3, MULIB_IMTUI_LAYOUT_CENTER,
                                 MULIB_IMTUI_LAYOUT_END, &r));
  expect_rect(r, -8, 8, 4, 3);
  assert(mulib_imtui_layout_align(area, 0, INT_MAX, MULIB_IMTUI_LAYOUT_STRETCH,
                                 MULIB_IMTUI_LAYOUT_START, &r));
  expect_rect(r, -10, 5, 9, 6);
  area = (mulib_imtui_rect_t){INT_MAX - 3, INT_MIN, 3, 0};
  assert(mulib_imtui_layout_align(area, 0, 5, MULIB_IMTUI_LAYOUT_END,
                                 MULIB_IMTUI_LAYOUT_CENTER, &r));
  expect_rect(r, INT_MAX, INT_MIN, 0, 0);
  assert(!mulib_imtui_layout_align(area, -1, 0, MULIB_IMTUI_LAYOUT_STRETCH,
                                  MULIB_IMTUI_LAYOUT_END, &r));
  assert(!mulib_imtui_layout_align(area, 0, 0, MULIB_IMTUI_LAYOUT_START,
                                  (mulib_imtui_layout_alignment_t)4, &r));
  expect_rect(r, INT_MAX, INT_MIN, 0, 0);
}

static void
split_constraints(void)
{
  mulib_imtui_layout_split_t s;
  mulib_imtui_rect_t area = {-1, 3, 10, 8};
  assert(mulib_imtui_layout_split(area, MULIB_IMTUI_LAYOUT_X, 2, 4, 3, 1, &s));
  expect_rect(s.first, -1, 3, 4, 8);
  expect_rect(s.divider, 3, 3, 2, 8);
  expect_rect(s.second, 5, 3, 4, 8);
  assert(mulib_imtui_layout_split(area, MULIB_IMTUI_LAYOUT_Y, INT_MAX, INT_MIN,
                                 4, 3, &s));
  expect_rect(s.first, -1, 3, 10, 4);
  expect_rect(s.divider, -1, 7, 10, 1);
  expect_rect(s.second, -1, 8, 10, 3);
  assert(mulib_imtui_layout_split(area, MULIB_IMTUI_LAYOUT_X, 3, INT_MAX,
                                 2, 2, &s));
  expect_rect(s.first, -1, 3, 5, 8);
  expect_rect(s.divider, 4, 3, 3, 8);
  expect_rect(s.second, 7, 3, 2, 8);
  assert(mulib_imtui_layout_split(area, MULIB_IMTUI_LAYOUT_X, 3, INT_MIN,
                                 6, INT_MAX, &s));
  expect_rect(s.first, -1, 3, 6, 8);
  expect_rect(s.divider, 5, 3, 0, 8);
  expect_rect(s.second, 5, 3, 4, 8);
  assert(mulib_imtui_layout_split(area, MULIB_IMTUI_LAYOUT_Y, INT_MAX, INT_MAX,
                                 INT_MAX, INT_MAX, &s));
  assert(!mulib_imtui_layout_split(area, MULIB_IMTUI_LAYOUT_Y, 1, 1, 1, -1, &s));
  expect_rect(s.first, -1, 3, 10, 8);
  expect_rect(s.divider, -1, 11, 10, 0);
  expect_rect(s.second, -1, 11, 10, 0);
}

static void
scroll_geometry(void)
{
  mulib_imtui_scroll_view_t view;
  mulib_imtui_rect_t r, viewport = {-2, 7, 5, 4};

  assert(mulib_imtui_scroll_step(8, 1, 4, 20) == 9);
  assert(mulib_imtui_scroll_step(-8, 3, 4, 20) == 3);
  assert(mulib_imtui_scroll_step(INT_MAX, -2, 4, 20) == 14);
  assert(mulib_imtui_scroll_step(INT_MAX, INT_MIN, 0, INT_MAX) == 0);
  assert(mulib_imtui_scroll_step(1, INT_MAX, 0, INT_MAX) == INT_MAX);
  assert(mulib_imtui_scroll_step(INT_MIN, INT_MAX, 4, 3) == 0);
  assert(mulib_imtui_scroll_step(0, 4, INT_MIN, INT_MIN) == 0);
  assert(mulib_imtui_scroll_step(0, 4, INT_MIN, INT_MAX) == 4);

  assert(mulib_imtui_scroll_viewport(viewport, 30, 3, INT_MAX, INT_MIN, &view));
  expect_rect(view.viewport, -2, 7, 5, 4);
  expect_rect(view.visible, 25, 0, 5, 3);
  assert(mulib_imtui_scroll_project(&view, (mulib_imtui_rect_t){26, -1, 2, 3}, &r));
  expect_rect(r, -1, 7, 2, 2);
  assert(mulib_imtui_scroll_project(&view, (mulib_imtui_rect_t){23, 2, 4, 9}, &r));
  expect_rect(r, -2, 9, 2, 1);
  assert(mulib_imtui_scroll_project(&view, (mulib_imtui_rect_t){INT_MIN, 9, 16, 2}, &r));
  expect_rect(r, -2, 10, 0, 0);
  assert(mulib_imtui_scroll_project(&view, (mulib_imtui_rect_t){40, -9, 8, 3}, &r));
  expect_rect(r, 3, 7, 0, 0);

  viewport = (mulib_imtui_rect_t){INT_MAX - 2, INT_MIN, 2, 9};
  assert(mulib_imtui_scroll_viewport(viewport, INT_MAX, INT_MAX, INT_MAX,
                                   INT_MAX, &view));
  expect_rect(view.visible, INT_MAX - 2, INT_MAX - 9, 2, 9);
  assert(mulib_imtui_scroll_project(&view,
                                   (mulib_imtui_rect_t){INT_MAX - 1, INT_MAX - 5,
                                                        INT_MAX, INT_MAX}, &r));
  expect_rect(r, INT_MAX - 1, INT_MIN + 4, 1, 5);
  assert(!mulib_imtui_scroll_project(&view, (mulib_imtui_rect_t){0, 0, 1, -1}, &r));
  expect_rect(r, INT_MAX - 1, INT_MIN + 4, 1, 5);
  assert(!mulib_imtui_scroll_viewport(viewport, -1, 2, 0, 0, &view));
  assert(!mulib_imtui_scroll_viewport((mulib_imtui_rect_t){0, INT_MAX, 0, 1},
                                    1, 2, 0, 0, &view));
  expect_rect(view.visible, INT_MAX - 2, INT_MAX - 9, 2, 9);

  viewport = (mulib_imtui_rect_t){7, -1, 0, 1};
  assert(mulib_imtui_scroll_viewport(viewport, INT_MAX, 0, INT_MAX, 1, &view));
  expect_rect(view.visible, INT_MAX, 0, 0, 0);
  assert(mulib_imtui_scroll_project(&view, (mulib_imtui_rect_t){0, 0, 1, 1}, &r));
  expect_rect(r, 7, -1, 0, 0);
}

static void
scroll_clipping(void)
{
  mulib_imtui_t ui;
  mulib_imtui_scroll_view_t view, empty, inner;
  mulib_imtui_rect_t item;
  mulib_term_cell_t cell = {'X', mulib_term_style_default()};
  mulib_imtui_rect_t parent = {1, 1, 4, 2};
  mulib_termkey_event_t mouse = {.type = MULIB_TERMKEY_MOUSE,
                                  .mouse_action = MULIB_TERMKEY_MOVE,
                                  .x = 5, .y = 1};
  const mulib_term_cell_t *cells;
  assert(mulib_imtui_init(&ui, 6, 4));
  assert(mulib_imtui_scroll_viewport((mulib_imtui_rect_t){3, 0, 8, 4},
                                    10, 10, 1, 2, &view));
  assert(mulib_imtui_scroll_viewport((mulib_imtui_rect_t){-5, -5, 1, 1},
                                    1, 1, 0, 0, &empty));
  assert(!mulib_imtui_scroll_begin(&ui, &view));

  assert(mulib_imtui_begin(&ui, NULL, 0));
  assert(mulib_imtui_clip_push(&ui, parent));
  assert(mulib_imtui_scroll_begin(&ui, &view));
  expect_rect(mulib_imtui_clip(&ui), 3, 1, 2, 2);
  assert(mulib_imtui_put(&ui, 3, 1, cell));
  assert(!mulib_imtui_put(&ui, 3, 0, cell));
  assert(!mulib_imtui_put(&ui, 2, 1, cell));
  assert(!mulib_imtui_put(&ui, 5, 1, cell));

  assert(mulib_imtui_scroll_begin(&ui, &empty));
  assert(mulib_imtui_clip(&ui).w == 0 && mulib_imtui_clip(&ui).h == 0);
  assert(!mulib_imtui_put(&ui, 3, 1, cell));
  mulib_imtui_scroll_end(&ui);
  expect_rect(mulib_imtui_clip(&ui), 3, 1, 2, 2);
  mulib_imtui_scroll_end(&ui);
  expect_rect(mulib_imtui_clip(&ui), 1, 1, 4, 2);
  assert(mulib_imtui_put(&ui, 2, 1, cell));
  mulib_imtui_clip_pop(&ui);
  expect_rect(mulib_imtui_clip(&ui), 0, 0, 6, 4);

  assert(mulib_imtui_scroll_begin(&ui, &view));
  expect_rect(mulib_imtui_clip(&ui), 3, 0, 3, 4);
  for (size_t i = 1; i < MULIB_IMTUI_STACK_CAPACITY; ++i)
    assert(mulib_imtui_scroll_begin(&ui, &view));
  assert(!mulib_imtui_scroll_begin(&ui, &empty));
  expect_rect(mulib_imtui_clip(&ui), 3, 0, 3, 4);
  for (size_t i = 0; i < MULIB_IMTUI_STACK_CAPACITY; ++i)
    mulib_imtui_scroll_end(&ui);
  expect_rect(mulib_imtui_clip(&ui), 0, 0, 6, 4);

  cells = mulib_imtui_cells(&ui);
  assert(cells[3].codepoint == ' ' && cells[11].codepoint == ' ');
  assert(cells[8].codepoint == 'X' && cells[9].codepoint == 'X');
  mulib_imtui_end(&ui);

  assert(mulib_imtui_scroll_viewport((mulib_imtui_rect_t){3, 0, 3, 4},
                                    5, 10, 1, 2, &inner));
  assert(mulib_imtui_scroll_project(&inner, (mulib_imtui_rect_t){0, 3, 5, 2}, &item));
  expect_rect(item, 3, 1, 3, 2);
  assert(mulib_imtui_begin(&ui, &mouse, 1));
  assert(mulib_imtui_clip_push(&ui, parent));
  assert(mulib_imtui_scroll_begin(&ui, &inner));
  assert(!(mulib_imtui_interact(&ui, 1, item, 1) & MULIB_IMTUI_HOVERED));
  mulib_imtui_scroll_end(&ui);
  mulib_imtui_clip_pop(&ui);
  mulib_imtui_end(&ui);

  mouse.x = 4;
  assert(mulib_imtui_begin(&ui, &mouse, 1));
  assert(mulib_imtui_clip_push(&ui, parent));
  assert(mulib_imtui_scroll_begin(&ui, &inner));
  assert(mulib_imtui_interact(&ui, 1, item, 1) & MULIB_IMTUI_HOVERED);
  mulib_imtui_scroll_end(&ui);
  mulib_imtui_clip_pop(&ui);
  mulib_imtui_end(&ui);
  mulib_imtui_free(&ui);
}

int
main(void)
{
  equal_tracks();
  equal_limits();
  weighted_tracks();
  padding_alignment();
  split_constraints();
  scroll_geometry();
  scroll_clipping();
  return 0;
}
