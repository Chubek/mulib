/* Complete, offline price-observation example. */
#define MULIB_IMPL_MONEY
#define MULIB_IMPL_PRICE_OBSERVATION
#include "mulib-price-observation.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_price_observation_t o = { 0 };
  strcpy (o.product, "widget");
  strcpy (o.merchant, "shop");
  assert (mulib_money_make (&o.price, 1299, "USD", 2));
  assert (mulib_price_observation_validate (&o));
  return 0;
}
