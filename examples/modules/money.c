/* Complete, offline money example. */
#define MULIB_IMPL_MONEY
#include "mulib-money.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_money_t a, b, total;
  assert (mulib_money_parse ("12.34", "USD", 2, &a));
  assert (mulib_money_make (&b, 100, "USD", 2));
  assert (mulib_money_add (a, b, &total) && total.minor == 1334);
  return 0;
}
