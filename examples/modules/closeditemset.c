#define MULIB_IMPL_FCA
#define MULIB_IMPL_NEXTCLOSURE
#define MULIB_IMPL_CLOSEDITEMSET
#include "mulib-closeditemset.h"
#include <assert.h>
#include <stdio.h>
static int
visit (uint64_t attrs, uint64_t objects, void *ctx)
{
  (void)ctx;
  printf ("intent=%llu extent=%llu\n", (unsigned long long)attrs,
          (unsigned long long)objects);
  return 1;
}
int
main (void)
{
  mulib_fca_t c;
  assert (mulib_fca_init (&c, 2, 2));
  assert (mulib_fca_set (&c, 0, 0, 1) && mulib_fca_set (&c, 1, 1, 1));
  assert (mulib_closeditemset_enumerate (&c, 1, visit, NULL) == 3);
  return 0;
}
