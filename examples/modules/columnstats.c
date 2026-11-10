#define MULIB_IMPL_COLUMNSTATS
#include "mulib-columnstats.h"
#include <assert.h>
int
main (void)
{
  mulib_columnstats_t s = { 0 };
  assert (mulib_columnstats_add (&s, 10, 1) && mulib_columnstats_add (&s, 20, 1));
  assert (mulib_columnstats_add (&s, 0, 0) && s.nulls == 1);
  assert (mulib_columnstats_may_match (&s, 15, 25));
  assert (!mulib_columnstats_may_match (&s, 21, 25));
  return 0;
}
