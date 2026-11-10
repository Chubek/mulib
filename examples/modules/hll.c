#define MULIB_IMPL_HLL
#include "mulib-hll.h"
#include <assert.h>
#include <stdio.h>
int
main (void)
{
  mulib_hll_t users = { 0 };
  for (unsigned i = 0; i < 1000; i++)
    {
      char key[16];
      int n = snprintf (key, sizeof (key), "user%u", i);
      mulib_hll_add (&users, key, (size_t)n);
    }
  double estimate = mulib_hll_estimate (&users);
  assert (estimate > 800 && estimate < 1200);
  printf ("Estimated distinct users: %.0f\n", estimate);
  return 0;
}
