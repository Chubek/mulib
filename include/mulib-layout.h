/** @file mulib-layout.h
 * @brief Finite weighted row/column rectangle layout, backend-neutral.
 * Define MULIB_IMPL_LAYOUT once. Does not render, wrap text or implement CSS.
 */
#ifndef MULIB_LAYOUT_H
#define MULIB_LAYOUT_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_layout_rect
  {
    double x, y, width, height;
  } mulib_layout_rect_t;
  enum
  {
    MULIB_LAYOUT_ROW = 0,
    MULIB_LAYOUT_COLUMN = 1
  };
  /** @brief Divide parent's main axis by nonnegative weights, inserting gap.
   * All inputs finite, nonnegative sizes/gap, positive sum for nonempty input.
   * 1 success, 0 invalid/overflow/OOM. Output unchanged on failure; aliasing
   * allowed. Zero weights receive zero main-axis length. Empty input succeeds.
   */
  int mulib_layout_distribute (mulib_layout_rect_t parent, int direction,
                               double gap, const double *weights, size_t count,
                               mulib_layout_rect_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LAYOUT) && !defined(MULIB_LAYOUT_IMPLEMENTED)
#define MULIB_LAYOUT_IMPLEMENTED
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
int
mulib_layout_distribute (mulib_layout_rect_t p, int dir, double gap,
                         const double *w, size_t n, mulib_layout_rect_t *out)
{
  double sum = 0, main, available, position = 0;
  size_t i;
  mulib_layout_rect_t *r;
  if ((dir != 0 && dir != 1) || !isfinite (p.x) || !isfinite (p.y)
      || !isfinite (p.width) || !isfinite (p.height) || p.width < 0
      || p.height < 0 || !isfinite (p.x + p.width)
      || !isfinite (p.y + p.height) || !isfinite (gap) || gap < 0 || (!w && n)
      || (!out && n) || n > SIZE_MAX / sizeof (*r))
    return 0;
  if (!n)
    return 1;
  for (i = 0; i < n; ++i)
    {
      if (!isfinite (w[i]) || w[i] < 0)
        return 0;
      sum += w[i];
    }
  if (!isfinite (sum) || sum <= 0)
    return 0;
  main = dir ? p.height : p.width;
  available = main - gap * (double)(n - 1);
  if (!isfinite (available) || available < 0)
    return 0;
  r = (mulib_layout_rect_t *)malloc (n * sizeof (*r));
  if (!r)
    return 0;
  for (i = 0; i < n; ++i)
    {
      double size = available * (w[i] / sum);
      r[i] = p;
      if (dir)
        {
          r[i].y += position;
          r[i].height = size;
        }
      else
        {
          r[i].x += position;
          r[i].width = size;
        }
      position += size;
      if (i + 1 < n)
        position += gap;
      if (!isfinite (position))
        {
          free (r);
          return 0;
        }
    }
  memcpy (out, r, n * sizeof (*r));
  free (r);
  return 1;
}
#endif
