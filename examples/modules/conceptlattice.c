#define MULIB_IMPL_FCA
#define MULIB_IMPL_NEXTCLOSURE
#define MULIB_IMPL_CONCEPTLATTICE
#include "mulib-conceptlattice.h"
#include <assert.h>
int
main (void)
{
  mulib_fca_t c;
  mulib_concept_t concepts[4];
  assert (mulib_fca_init (&c, 2, 2));
  assert (mulib_fca_set (&c, 0, 0, 1) && mulib_fca_set (&c, 1, 1, 1));
  assert (mulib_conceptlattice_enumerate (&c, concepts, 4) == 4);
  assert (mulib_conceptlattice_leq (concepts[3], concepts[0]));
  return 0;
}
