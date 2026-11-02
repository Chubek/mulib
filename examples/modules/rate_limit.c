#define MULIB_IMPL_RATE_LIMIT
#include "mulib-rate_limit.h"
#include <assert.h>
int
main (void)
{
  mulib_rate_limit_t bucket;
  assert (mulib_rate_limit_init (&bucket, 2, 3, 0));
  assert (mulib_rate_limit_take (&bucket, 3, 0));
  assert (!mulib_rate_limit_take (&bucket, 1, 0));
  assert (mulib_rate_limit_take (&bucket, 2, 1));
  return 0;
}
