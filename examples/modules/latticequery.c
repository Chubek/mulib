#define MULIB_IMPL_FCA
#define MULIB_IMPL_NEXTCLOSURE
#define MULIB_IMPL_CONCEPTLATTICE
#define MULIB_IMPL_LATTICEQUERY
#include "mulib-latticequery.h"
#include <assert.h>
int
main (void)
{
  mulib_fca_t c;
  mulib_concept_t concepts[4];
  size_t indices[4];
  assert (mulib_fca_init (&c, 2, 2));
  assert (mulib_fca_set (&c, 0, 0, 1) && mulib_fca_set (&c, 1, 1, 1));
  size_t n = mulib_conceptlattice_enumerate (&c, concepts, 4);
  assert (n == 4);
  assert (mulib_latticequery_filter (concepts, n, 1, 1, indices, 4) == 1);
  assert (concepts[indices[0]].intent == 1);
  return 0;
}
