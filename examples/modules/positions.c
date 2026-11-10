#define MULIB_IMPL_POSITIONS
#include "mulib-positions.h"
#include <assert.h>
int
main (void)
{
  mulib_positions_t p = { 0 };
  assert (mulib_positions_add (&p, 8) && mulib_positions_add (&p, 2));
  assert (mulib_positions_add (&p, 8) && p.count == 2 && p.items[0] == 2);
  mulib_positions_free (&p);
  return 0;
}
