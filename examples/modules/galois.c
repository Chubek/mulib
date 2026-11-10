#define MULIB_IMPL_FCA
#define MULIB_IMPL_GALOIS
#include "mulib-galois.h"
#include <assert.h>
int
main (void)
{
  mulib_fca_t c;
  assert (mulib_fca_init (&c, 2, 1));
  assert (mulib_fca_set (&c, 0, 0, 1) && mulib_fca_set (&c, 1, 0, 1));
  assert (mulib_galois_objects (&c, 1) == 3
          && mulib_galois_object_closure (&c, 1) == 3);
  return 0;
}
