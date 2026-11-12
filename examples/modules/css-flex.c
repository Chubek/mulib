/* Complete, offline css-flex example. */
#define MULIB_IMPL_CSS_BOX
#define MULIB_IMPL_CSS_FLEX
#include "mulib-css-flex.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_css_box_t parent = { 0, 0, 100, 20 }, boxes[2];
  double weights[] = { 1, 3 };
  assert (mulib_css_flex_distribute (parent, weights, 2, 0, 1, boxes));
  assert (boxes[0].width == 25 && boxes[1].width == 75);
  return 0;
}
