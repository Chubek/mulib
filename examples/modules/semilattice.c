#define MULIB_IMPL_SEMILATTICE
#include "mulib-semilattice.h"
#include <assert.h>
int
main (void)
{
  uint64_t merged = mulib_semilattice_join (3, 6);
  assert (merged == 7 && mulib_semilattice_leq (3, merged));
  assert (mulib_semilattice_meet (3, 6) == 2);
  return 0;
}
