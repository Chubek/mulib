#define MULIB_IMPL_DATUM
#define MULIB_IMPL_QUERYEXPR
#include "mulib-queryexpr.h"
#include <assert.h>
int
main (void)
{
  mulib_datum_t a = { 0 }, b = { 0 };
  assert (mulib_queryexpr_compare (&a, &b) == 2);
  assert (mulib_datum_set_i64 (&a, 10) && mulib_datum_set_i64 (&b, 20));
  assert (mulib_queryexpr_compare (&a, &b) == -1);
  mulib_datum_free (&a);
  mulib_datum_free (&b);
  return 0;
}
