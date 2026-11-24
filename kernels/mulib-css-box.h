/** @file mulib-css-box.h
 * @brief Backend-neutral block box geometry and vertical flow placement.
 * C11 single-header module. Define MULIB_IMPL_CSS_BOX in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_CSS_BOX_H
#define MULIB_CSS_BOX_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_css_box
  {
    double x, y, width, height;
  } mulib_css_box_t;
  /** @brief Finite coordinates, nonnegative extents and finite far edges. */
  int mulib_css_box_valid (mulib_css_box_t box);
  /** @brief See manual/mulib-css-box.md for this operation's contract. */
  int mulib_css_box_vertical (mulib_css_box_t parent, const double *heights,
                              size_t n, double gap, mulib_css_box_t *out);
  /** @brief See manual/mulib-css-box.md for this operation's contract. */
  int mulib_css_box_contains (mulib_css_box_t box, double x, double y);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSS_BOX_H */
#ifdef MULIB_IMPL_CSS_BOX
#ifndef MULIB_IMPL_CSS_BOX_ONCE
#define MULIB_IMPL_CSS_BOX_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_css_box_valid (mulib_css_box_t b)
{
  return isfinite (b.x) && isfinite (b.y) && isfinite (b.width)
         && isfinite (b.height) && b.width >= 0 && b.height >= 0
         && isfinite (b.x + b.width) && isfinite (b.y + b.height);
}
int
mulib_css_box_vertical (mulib_css_box_t p, const double *hs, size_t n,
                        double gap, mulib_css_box_t *out)
{
  if ((n && (!hs || !out)) || !mulib_css_box_valid (p) || !isfinite (gap)
      || gap < 0)
    return 0;
  long double used = 0;
  for (size_t i = 0; i < n; i++)
    {
      if (!isfinite (hs[i]) || hs[i] < 0)
        return 0;
      if (i)
        used += gap;
      used += hs[i];
      if (used > p.height)
        return 0;
    }
  long double y = p.y;
  for (size_t i = 0; i < n; i++)
    {
      out[i] = (mulib_css_box_t){ p.x, (double)y, p.width, hs[i] };
      y += (long double)hs[i] + gap;
    }
  return 1;
}
int
mulib_css_box_contains (mulib_css_box_t b, double x, double y)
{
  return mulib_css_box_valid (b) && isfinite (x) && isfinite (y) && x >= b.x
         && x < b.x + b.width && y >= b.y && y < b.y + b.height;
}
#endif
#endif
