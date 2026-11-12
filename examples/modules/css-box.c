/* Complete, offline css-box example. */
#define MULIB_IMPL_CSS_BOX
#include "mulib-css-box.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_css_box_t parent = { 0, 0, 100, 100 }, boxes[2];
  double heights[] = { 20, 30 };
  assert (mulib_css_box_vertical (parent, heights, 2, 5, boxes)
          && boxes[1].y == 25);
  assert (mulib_css_box_contains (boxes[0], 1, 1));
  return 0;
}
