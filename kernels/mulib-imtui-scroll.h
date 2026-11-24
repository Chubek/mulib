/**
 * @file mulib-imtui-scroll.h
 * @brief Integer scroll offsets, viewport projection and scoped clipping.
 *
 * Define MULIB_IMPL_IMTUI_SCROLL in one translation unit and link the ImTUI
 * core. The application owns offsets and decides when to handle scroll input.
 */
#ifndef MULIB_IMTUI_SCROLL_H
#define MULIB_IMTUI_SCROLL_H

#include "mulib-imtui-core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_imtui_scroll_view {
  mulib_imtui_rect_t viewport; /**< Terminal-space viewport (clip request). */
  mulib_imtui_rect_t visible;  /**< Visible rectangle in content coordinates. */
} mulib_imtui_scroll_view_t;

/** Clamp offset to [0, max(content_extent - viewport_extent, 0)], then apply
 * delta with saturation and clamp again. Negative extents are treated as zero. */
int mulib_imtui_scroll_step(int offset, int delta, int viewport_extent,
                           int content_extent);

/** Compute a viewport with clamped offsets; viewport and content dimensions
 * must be nonnegative and the exclusive edges of viewport must fit in int.
 * Content begins at (0,0). Its visible portion can be smaller than viewport.
 * Returns 1, or 0 without changing out. No terminal or parent clip is applied. */
int mulib_imtui_scroll_viewport(mulib_imtui_rect_t viewport, int content_w,
                               int content_h, int offset_x, int offset_y,
                               mulib_imtui_scroll_view_t *out);

/** Intersect a content-space rectangle with view's visible portion, then map
 * it to terminal coordinates. A disjoint axis produces zero extent at its
 * nearest visible edge. A rectangle may extend offscreen or outside content,
 * but its extents must be nonnegative. Returns 1 (including empty results)
 * or 0 without changing out. Does not apply the terminal or parent clip. */
int mulib_imtui_scroll_project(const mulib_imtui_scroll_view_t *view,
                              mulib_imtui_rect_t content,
                              mulib_imtui_rect_t *out);

/** Push the viewport as a clip, intersecting it with the core's current clip.
 * This doesn't shift the draw or hit-test origin. Pop with scroll_end only
 * after begin succeeds, even if the viewport ended up fully clipped. */
int mulib_imtui_scroll_begin(mulib_imtui_t *ui,
                            const mulib_imtui_scroll_view_t *view);
void mulib_imtui_scroll_end(mulib_imtui_t *ui);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_IMTUI_SCROLL) && !defined(MULIB_IMTUI_SCROLL_IMPLEMENTED)
#define MULIB_IMTUI_SCROLL_IMPLEMENTED

#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

static int
mulib_imtui_scroll_valid_(mulib_imtui_rect_t rect)
{
  return rect.w >= 0 && rect.h >= 0 &&
    rect.x <= INT_MAX - rect.w && rect.y <= INT_MAX - rect.h;
}

static int
mulib_imtui_scroll_view_valid_(const mulib_imtui_scroll_view_t *view)
{
  return view && mulib_imtui_scroll_valid_(view->viewport) &&
    mulib_imtui_scroll_valid_(view->visible) &&
    view->visible.x >= 0 && view->visible.y >= 0 &&
    view->visible.w <= view->viewport.w && view->visible.h <= view->viewport.h;
}

int
mulib_imtui_scroll_step(int offset, int delta, int viewport_extent,
                       int content_extent)
{
  int max_offset;
  if (viewport_extent < 0)
    viewport_extent = 0;
  max_offset = content_extent > viewport_extent ?
    content_extent - viewport_extent : 0;
  if (offset < 0)
    offset = 0;
  if (offset > max_offset)
    offset = max_offset;
  if (delta < -offset)
    return 0;
  if (delta > max_offset - offset)
    return max_offset;
  return offset + delta;
}

int
mulib_imtui_scroll_viewport(mulib_imtui_rect_t viewport, int content_w,
                           int content_h, int offset_x, int offset_y,
                           mulib_imtui_scroll_view_t *out)
{
  mulib_imtui_rect_t visible;
  if (!out || content_w < 0 || content_h < 0 ||
      !mulib_imtui_scroll_valid_(viewport))
    return 0;
  visible.x = mulib_imtui_scroll_step(offset_x, 0, viewport.w, content_w);
  visible.y = mulib_imtui_scroll_step(offset_y, 0, viewport.h, content_h);
  visible.w = viewport.w < content_w ? viewport.w : content_w;
  visible.h = viewport.h < content_h ? viewport.h : content_h;
  out->viewport = viewport;
  out->visible = visible;
  return 1;
}

/* Clamp both source edges into the visible range. Saturating an oversized
 * exclusive edge at INT_MAX is exact here because the clip ends by INT_MAX. */
static void
mulib_imtui_scroll_project_(int origin, int extent, int low, int length,
                            int *offset, int *result_extent)
{
  int high = low + length;
  int end = origin > INT_MAX - extent ? INT_MAX : origin + extent;
  int start_clamped = origin < low ? low : origin > high ? high : origin;
  int end_clamped = end < low ? low : end > high ? high : end;
  *offset = start_clamped - low;
  *result_extent = end_clamped - start_clamped;
}

int
mulib_imtui_scroll_project(const mulib_imtui_scroll_view_t *view,
                          mulib_imtui_rect_t content, mulib_imtui_rect_t *out)
{
  int x, y;
  if (!out || content.w < 0 || content.h < 0 ||
      !mulib_imtui_scroll_view_valid_(view))
    return 0;
  mulib_imtui_scroll_project_(content.x, content.w, view->visible.x,
                             view->visible.w, &x, &content.w);
  mulib_imtui_scroll_project_(content.y, content.h, view->visible.y,
                             view->visible.h, &y, &content.h);
  content.x = view->viewport.x + x;
  content.y = view->viewport.y + y;
  *out = content;
  return 1;
}

int
mulib_imtui_scroll_begin(mulib_imtui_t *ui, const mulib_imtui_scroll_view_t *view)
{
  return ui && mulib_imtui_scroll_view_valid_(view) &&
    mulib_imtui_clip_push(ui, view->viewport);
}

void
mulib_imtui_scroll_end(mulib_imtui_t *ui)
{
  if (ui)
    mulib_imtui_clip_pop(ui);
}

#ifdef __cplusplus
}
#endif
#endif
