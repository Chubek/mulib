#define MULIB_IMPL_QUERYAGGREGATE
#include "mulib-queryaggregate.h"
#include <assert.h>
int
main (void)
{
  mulib_queryaggregate_t a = { 0 }, b = { 0 };
  assert (mulib_queryaggregate_add (&a, 10) && mulib_queryaggregate_add (&b, 20));
  assert (mulib_queryaggregate_merge (&a, &b) && a.count == 2 && a.mean == 15
          && a.min == 10 && a.max == 20);
  return 0;
}
