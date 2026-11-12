/* Complete, offline dom-range example. */
#define MULIB_IMPL_DOM_RANGE
#include "mulib-dom-range.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_dom_range_t a, b, intersection;
  assert (mulib_dom_range_make (1, 5, 10, 1, &a));
  assert (mulib_dom_range_shift (a, 2, 10, &b));
  assert (mulib_dom_range_intersect (a, b, &intersection)
          && intersection.start == 3);
  return 0;
}
