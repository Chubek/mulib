#define MULIB_IMPL_LATTICE
#include "mulib-lattice.h"
#include <assert.h>
int
main (void)
{
  mulib_lattice_t l = { 0 };
  l.attributes = 3;
  assert (mulib_lattice_add_rule (&l, 1, 2) && mulib_lattice_add_rule (&l, 2, 4));
  assert (mulib_lattice_closure (&l, 1) == 7 && mulib_lattice_meet (&l, 1, 2) == 6);
  mulib_lattice_free (&l);
  return 0;
}
