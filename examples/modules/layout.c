#define MULIB_IMPL_LAYOUT
#include "mulib-layout.h"
#include <assert.h>
int
main (void)
{
  mulib_layout_rect_t p = { 0, 0, 100, 20 }, r[2];
  double weights[] = { 1, 3 };
  assert (mulib_layout_distribute (p, MULIB_LAYOUT_ROW, 4, weights, 2, r));
  assert (r[0].width == 24 && r[1].x == 28);
  return 0;
}
