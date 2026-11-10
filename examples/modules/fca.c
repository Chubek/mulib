#define MULIB_IMPL_FCA
#include "mulib-fca.h"
#include <assert.h>
int
main (void)
{
  mulib_fca_t c;
  assert (mulib_fca_init (&c, 2, 2));
  assert (mulib_fca_set (&c, 0, 0, 1) && mulib_fca_set (&c, 0, 1, 1));
  assert (mulib_fca_set (&c, 1, 0, 1));
  assert (mulib_fca_extent (&c, 1) == 3 && mulib_fca_closure (&c, 2) == 3);
  return 0;
}
