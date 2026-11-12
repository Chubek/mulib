/* Complete, offline price-normalize example. */
#define MULIB_IMPL_MONEY
#define MULIB_IMPL_PRICE_NORMALIZE
#include "mulib-price-normalize.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_price_normalize_input_t in = { 0 };
  mulib_money_t total, unit;
  assert (mulib_money_make (&in.base, 1000, "USD", 2));
  assert (mulib_money_make (&in.shipping, 100, "USD", 2));
  in.quantity = 2;
  assert (mulib_price_normalize (&in, &total, &unit) && unit.minor == 550);
  return 0;
}
