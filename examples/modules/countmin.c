#define MULIB_IMPL_COUNTMIN
#include "mulib-countmin.h"
#include <assert.h>
int
main (void)
{
  mulib_countmin_t counts = { 0 };
  assert (mulib_countmin_init (&counts, 1024, 4));
  assert (mulib_countmin_add (&counts, "apple", 5, 3));
  assert (mulib_countmin_add (&counts, "apple", 5, 2));
  assert (mulib_countmin_estimate (&counts, "apple", 5) >= 5);
  mulib_countmin_free (&counts);
  return 0;
}
