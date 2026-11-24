/** @file mulib-css-flex.h
 * @brief Checked weighted one-axis flex distribution with gaps.
 * C11 single-header module. Define MULIB_IMPL_CSS_FLEX in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_CSS_FLEX_H
#define MULIB_CSS_FLEX_H
#include "mulib-css-box.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief See manual/mulib-css-flex.md for this operation's contract. */
  int mulib_css_flex_distribute (mulib_css_box_t parent, const double *weights,
                                 size_t n, double gap, int row,
                                 mulib_css_box_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSS_FLEX_H */
#ifdef MULIB_IMPL_CSS_FLEX
#ifndef MULIB_IMPL_CSS_FLEX_ONCE
#define MULIB_IMPL_CSS_FLEX_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_css_flex_distribute (mulib_css_box_t p, const double *w, size_t n,
                           double gap, int row, mulib_css_box_t *out)
{
  if ((n && (!w || !out)) || !isfinite (gap) || gap < 0
      || !mulib_css_box_valid (p) || n > 1000000)
    return 0;
  long double total = 0;
  for (size_t i = 0; i < n; i++)
    {
      if (!isfinite (w[i]) || w[i] < 0)
        return 0;
      total += w[i];
    }
  if (n && (!isfinite (total) || !total))
    return 0;
  long double available
      = (row ? p.width : p.height) - (long double)(n ? n - 1 : 0) * gap;
  if (!isfinite (available) || available < 0)
    return 0;
  long double offset = 0;
  for (size_t i = 0; i < n; i++)
    {
      double len = (double)(available * ((long double)w[i] / total));
      out[i] = row ? (mulib_css_box_t){ (double)(p.x + offset), p.y, len,
                                        p.height }
                   : (mulib_css_box_t){ p.x, (double)(p.y + offset), p.width,
                                        len };
      offset += (long double)len + gap;
    }
  return 1;
}
#endif
#endif
