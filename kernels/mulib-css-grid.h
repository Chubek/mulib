/** @file mulib-css-grid.h
 * @brief Checked uniform grid cell geometry generator.
 * C11 single-header module. Define MULIB_IMPL_CSS_GRID in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_CSS_GRID_H
#define MULIB_CSS_GRID_H
#include "mulib-css-box.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief See manual/mulib-css-grid.md for this operation's contract. */
  int mulib_css_grid_cells (mulib_css_box_t parent, size_t rows,
                            size_t columns, double column_gap, double row_gap,
                            mulib_css_box_t *out, size_t cap);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSS_GRID_H */
#ifdef MULIB_IMPL_CSS_GRID
#ifndef MULIB_IMPL_CSS_GRID_ONCE
#define MULIB_IMPL_CSS_GRID_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_css_grid_cells (mulib_css_box_t p, size_t rows, size_t cols, double cg,
                      double rg, mulib_css_box_t *out, size_t cap)
{
  if (!rows || !cols || rows > SIZE_MAX / cols || rows * cols > cap || !out
      || !isfinite (cg) || !isfinite (rg) || cg < 0 || rg < 0
      || !mulib_css_box_valid (p))
    return 0;
  long double w = ((long double)p.width - (cols - 1) * (long double)cg) / cols,
              h
              = ((long double)p.height - (rows - 1) * (long double)rg) / rows;
  if (!isfinite (w) || !isfinite (h) || w < 0 || h < 0)
    return 0;
  for (size_t r = 0; r < rows; r++)
    for (size_t c = 0; c < cols; c++)
      out[r * cols + c] = (mulib_css_box_t){ (double)(p.x + c * (w + cg)),
                                             (double)(p.y + r * (h + rg)),
                                             (double)w, (double)h };
  return 1;
}
#endif
#endif
