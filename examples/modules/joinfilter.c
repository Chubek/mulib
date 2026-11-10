#define MULIB_IMPL_JOINFILTER
#include "mulib-joinfilter.h"
#include <assert.h>
int
main (void)
{
  mulib_joinfilter_t filter = { 0 };
  assert (mulib_joinfilter_init (&filter, 8192, 123));
  mulib_joinfilter_add (&filter, "customer-42", 11);
  assert (mulib_joinfilter_maybe (&filter, "customer-42", 11));
  /* A positive result still requires an exact join-key comparison. */
  mulib_joinfilter_free (&filter);
  return 0;
}
