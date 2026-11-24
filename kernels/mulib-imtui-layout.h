/**
 * @file mulib-imtui-layout.h
 * @brief Allocation-free integer-cell layouts for immediate-mode TUI.
 *
 * Define MULIB_IMPL_IMTUI_LAYOUT in one translation unit. Geometry uses
 * half-open rectangles: extents must be nonnegative and the exclusive right
 * and bottom edges must fit in int. Empty areas and empty tracks are valid.
 * All functions return 1 on success, or 0 without changing outputs.
 */
#ifndef MULIB_IMTUI_LAYOUT_H
#define MULIB_IMTUI_LAYOUT_H

#include "mulib-imtui-core.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum mulib_imtui_layout_axis {
  MULIB_IMTUI_LAYOUT_X,
  MULIB_IMTUI_LAYOUT_Y
} mulib_imtui_layout_axis_t;

typedef enum mulib_imtui_layout_alignment {
  MULIB_IMTUI_LAYOUT_START,
  MULIB_IMTUI_LAYOUT_CENTER,
  MULIB_IMTUI_LAYOUT_END,
  MULIB_IMTUI_LAYOUT_STRETCH
} mulib_imtui_layout_alignment_t;

typedef struct mulib_imtui_layout_split {
  mulib_imtui_rect_t first, divider, second;
} mulib_imtui_layout_split_t;

/** A row arranges tracks left to right; a column arranges them top to bottom.
 * Indices are zero-based. For two or more tracks the nonnegative gap is
 * capped at extent / (count - 1). Divide the usable extent E after gaps
 * evenly, giving the first E % count tracks one extra cell. Constant time. */
int mulib_imtui_layout_row(mulib_imtui_rect_t area, int gap, size_t count,
                          size_t index, mulib_imtui_rect_t *out);
int mulib_imtui_layout_column(mulib_imtui_rect_t area, int gap, size_t count,
                             size_t index, mulib_imtui_rect_t *out);

/** Equal tracks on both axes, with the same gap and remainder rules. Neither
 * rows*columns nor flattened indices are needed. */
int mulib_imtui_layout_grid(mulib_imtui_rect_t area, int x_gap, int y_gap,
                           size_t columns, size_t rows, size_t column,
                           size_t row, mulib_imtui_rect_t *out);

/** Write count weighted tracks along axis to out (disjoint from weights).
 * The gap rule is the same as for rows/columns. For usable extent E, the
 * boundary after a track is floor(E * cumulative_weight / total_weight)
 * before gaps are added. Rounding therefore differs from equal-track
 * layout. Zero-weight tracks receive zero cells; a zero or unrepresentable
 * total weight fails. out must have room for count rectangles.
 */
int mulib_imtui_layout_weighted(mulib_imtui_rect_t area,
                               mulib_imtui_layout_axis_t axis, int gap,
                               const unsigned *weights, size_t count,
                               mulib_imtui_rect_t *out);

/** Inset by nonnegative padding; if space runs out, left wins over right
 * and top wins over bottom. No padding is allowed to extend outside area. */
int mulib_imtui_layout_pad(mulib_imtui_rect_t area, int left, int top,
                          int right, int bottom, mulib_imtui_rect_t *out);

/** Place the nonnegative requested size within area, shrinking it if needed.
 * STRETCH ignores the requested size along that axis. CENTER leaves any odd
 * extra cell on the right/bottom. */
int mulib_imtui_layout_align(mulib_imtui_rect_t area, int width, int height,
                            mulib_imtui_layout_alignment_t x_align,
                            mulib_imtui_layout_alignment_t y_align,
                            mulib_imtui_rect_t *out);

/** Two panes and a divider. Honor the nonnegative pane minima before the
 * nonnegative gap; if the minima do not fit, first_min takes priority.
 * preferred_first (which may be negative) is then clamped to the feasible
 * range. All three rectangles span the cross axis and tile the input. */
int mulib_imtui_layout_split(mulib_imtui_rect_t area,
                            mulib_imtui_layout_axis_t axis, int gap,
                            int preferred_first, int first_min, int second_min,
                            mulib_imtui_layout_split_t *out);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_IMTUI_LAYOUT) && !defined(MULIB_IMTUI_LAYOUT_IMPLEMENTED)
#define MULIB_IMTUI_LAYOUT_IMPLEMENTED

#include <limits.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static int
mulib_imtui_layout_valid_(mulib_imtui_rect_t r)
{
  return r.w >= 0 && r.h >= 0 && r.x <= INT_MAX - r.w && r.y <= INT_MAX - r.h;
}

static int
mulib_imtui_layout_axis_valid_(mulib_imtui_layout_axis_t axis)
{
  return axis == MULIB_IMTUI_LAYOUT_X || axis == MULIB_IMTUI_LAYOUT_Y;
}

static int
mulib_imtui_layout_min_(int a, int b)
{
  return a < b ? a : b;
}

static int
mulib_imtui_layout_gap_(int extent, int gap, uintmax_t separators)
{
  uintmax_t limit;
  if (!separators)
    return 0;
  limit = (uintmax_t)extent / separators;
  return (uintmax_t)gap > limit ? (int)limit : gap;
}

static void
mulib_imtui_layout_equal_(int extent, int gap, uintmax_t count,
                          uintmax_t index, int *offset, int *length)
{
  uintmax_t usable, base, extra;
  gap = mulib_imtui_layout_gap_(extent, gap, count - 1);
  usable = (uintmax_t)extent - (uintmax_t)gap * (count - 1);
  base = usable / count;
  extra = usable % count;
  /* Both components fit within extent, even when count exceeds extent. */
  *offset = (int)(index * (base + (uintmax_t)gap) +
                  (index < extra ? index : extra));
  *length = (int)(base + (index < extra));
}

int
mulib_imtui_layout_row(mulib_imtui_rect_t area, int gap, size_t count,
                      size_t index, mulib_imtui_rect_t *out)
{
  int offset;
  if (!out || gap < 0 || index >= count || !mulib_imtui_layout_valid_(area))
    return 0;
  mulib_imtui_layout_equal_(area.w, gap, count, index, &offset, &area.w);
  area.x += offset;
  *out = area;
  return 1;
}

int
mulib_imtui_layout_column(mulib_imtui_rect_t area, int gap, size_t count,
                         size_t index, mulib_imtui_rect_t *out)
{
  int offset;
  if (!out || gap < 0 || index >= count || !mulib_imtui_layout_valid_(area))
    return 0;
  mulib_imtui_layout_equal_(area.h, gap, count, index, &offset, &area.h);
  area.y += offset;
  *out = area;
  return 1;
}

int
mulib_imtui_layout_grid(mulib_imtui_rect_t area, int x_gap, int y_gap,
                       size_t columns, size_t rows, size_t column,
                       size_t row, mulib_imtui_rect_t *out)
{
  if (!out || !mulib_imtui_layout_row(area, x_gap, columns, column, &area))
    return 0;
  return mulib_imtui_layout_column(area, y_gap, rows, row, out);
}

/* Compute floor(extent * part / total) exactly. If multiplication would
 * overflow, accumulate the quotient and remainder one extent bit at a time.
 * The quotient never exceeds extent; modular additions never wrap. */
static int
mulib_imtui_layout_share_(int extent, uintmax_t part, uintmax_t total)
{
  uintmax_t bits = (uintmax_t)extent, mask = 1, q = 0, r = 0;
  if (!part)
    return 0;
  if (part == total)
    return extent;
  if (bits <= UINTMAX_MAX / part)
    return (int)(bits * part / total);
  while (mask <= bits / 2)
    mask *= 2;
  for (; mask; mask >>= 1) {
    q *= 2;
    if (r >= total - r) {
      r -= total - r;
      ++q;
    } else
      r += r;
    if (bits & mask) {
      if (r >= total - part) {
        r -= total - part;
        ++q;
      } else
        r += part;
    }
  }
  return (int)q;
}

static mulib_imtui_rect_t
mulib_imtui_layout_slice_(mulib_imtui_rect_t area, mulib_imtui_layout_axis_t axis,
                          int offset, int length)
{
  if (axis == MULIB_IMTUI_LAYOUT_X) {
    area.x += offset;
    area.w = length;
  } else {
    area.y += offset;
    area.h = length;
  }
  return area;
}

int
mulib_imtui_layout_weighted(mulib_imtui_rect_t area,
                           mulib_imtui_layout_axis_t axis, int gap,
                           const unsigned *weights, size_t count,
                           mulib_imtui_rect_t *out)
{
  uintmax_t total = 0, prefix = 0;
  int extent, usable, before = 0;
  size_t i;

  if (!out || !weights || !count || gap < 0 ||
      count > SIZE_MAX / sizeof *out || count > SIZE_MAX / sizeof *weights ||
      !mulib_imtui_layout_axis_valid_(axis) || !mulib_imtui_layout_valid_(area))
    return 0;
  for (i = 0; i < count; ++i) {
    if (UINTMAX_MAX - total < weights[i])
      return 0;
    total += weights[i];
  }
  if (!total)
    return 0;

  extent = axis == MULIB_IMTUI_LAYOUT_X ? area.w : area.h;
  gap = mulib_imtui_layout_gap_(extent, gap, (uintmax_t)count - 1);
  usable = extent - (int)((uintmax_t)gap * (count - 1));
  for (i = 0; i < count; ++i) {
    int after, offset;
    prefix += weights[i];
    after = mulib_imtui_layout_share_(usable, prefix, total);
    offset = before + (int)((uintmax_t)gap * i);
    out[i] = mulib_imtui_layout_slice_(area, axis, offset, after - before);
    before = after;
  }
  return 1;
}

int
mulib_imtui_layout_pad(mulib_imtui_rect_t area, int left, int top,
                      int right, int bottom, mulib_imtui_rect_t *out)
{
  if (!out || left < 0 || top < 0 || right < 0 || bottom < 0 ||
      !mulib_imtui_layout_valid_(area))
    return 0;
  left = mulib_imtui_layout_min_(left, area.w);
  top = mulib_imtui_layout_min_(top, area.h);
  area.x += left;
  area.y += top;
  area.w -= left;
  area.h -= top;
  area.w -= mulib_imtui_layout_min_(right, area.w);
  area.h -= mulib_imtui_layout_min_(bottom, area.h);
  *out = area;
  return 1;
}

static int
mulib_imtui_layout_align_(mulib_imtui_layout_alignment_t align, int extent,
                          int *length, int *offset)
{
  *length = mulib_imtui_layout_min_(*length, extent);
  *offset = 0;
  switch (align) {
  case MULIB_IMTUI_LAYOUT_START:
    return 1;
  case MULIB_IMTUI_LAYOUT_CENTER:
    *offset = (extent - *length) / 2;
    return 1;
  case MULIB_IMTUI_LAYOUT_END:
    *offset = extent - *length;
    return 1;
  case MULIB_IMTUI_LAYOUT_STRETCH:
    *length = extent;
    return 1;
  default:
    return 0;
  }
}

int
mulib_imtui_layout_align(mulib_imtui_rect_t area, int width, int height,
                        mulib_imtui_layout_alignment_t x_align,
                        mulib_imtui_layout_alignment_t y_align,
                        mulib_imtui_rect_t *out)
{
  int dx, dy;
  if (!out || width < 0 || height < 0 || !mulib_imtui_layout_valid_(area) ||
      !mulib_imtui_layout_align_(x_align, area.w, &width, &dx) ||
      !mulib_imtui_layout_align_(y_align, area.h, &height, &dy))
    return 0;
  area.x += dx;
  area.y += dy;
  area.w = width;
  area.h = height;
  *out = area;
  return 1;
}

int
mulib_imtui_layout_split(mulib_imtui_rect_t area,
                        mulib_imtui_layout_axis_t axis, int gap,
                        int preferred_first, int first_min, int second_min,
                        mulib_imtui_layout_split_t *out)
{
  int extent, first_max, first;
  if (!out || gap < 0 || first_min < 0 || second_min < 0 ||
      !mulib_imtui_layout_axis_valid_(axis) || !mulib_imtui_layout_valid_(area))
    return 0;
  extent = axis == MULIB_IMTUI_LAYOUT_X ? area.w : area.h;
  first_min = mulib_imtui_layout_min_(first_min, extent);
  second_min = mulib_imtui_layout_min_(second_min, extent - first_min);
  gap = mulib_imtui_layout_min_(gap, extent - first_min - second_min);
  first_max = extent - gap - second_min;
  first = preferred_first < first_min ? first_min :
    mulib_imtui_layout_min_(preferred_first, first_max);

  out->first = mulib_imtui_layout_slice_(area, axis, 0, first);
  out->divider = mulib_imtui_layout_slice_(area, axis, first, gap);
  out->second = mulib_imtui_layout_slice_(area, axis, first + gap,
                                        extent - first - gap);
  return 1;
}

#ifdef __cplusplus
}
#endif
#endif
