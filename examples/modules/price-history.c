/* Complete, offline price-history example. */
#define MULIB_IMPL_MONEY
#define MULIB_IMPL_PRICE_HISTORY
#define MULIB_IMPL_PRICE_OBSERVATION
#include "mulib-price-history.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_price_history_t h = { 0 };
  mulib_price_observation_t o = { 0 };
  strcpy (o.product, "widget");
  strcpy (o.merchant, "shop");
  assert (mulib_money_make (&o.price, 100, "USD", 2));
  assert (mulib_price_history_add (&h, &o));
  assert (mulib_price_history_range (&h, 0, 10, NULL, 0) == 1);
  mulib_price_history_free (&h);
  return 0;
}
