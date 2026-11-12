/* Complete, offline css-grid example. */
#define MULIB_IMPL_CSS_BOX
#define MULIB_IMPL_CSS_GRID
#include "mulib-css-grid.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_css_box_t parent = { 0, 0, 100, 100 }, cells[4];
  assert (mulib_css_grid_cells (parent, 2, 2, 0, 0, cells, 4));
  assert (cells[3].x == 50 && cells[3].y == 50);
  return 0;
}
