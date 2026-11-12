/* Complete, offline price-demand example. */
#define MULIB_IMPL_PRICE_DEMAND
#include "mulib-price-demand.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  double prices[] = { 1, 2, 3 }, units[] = { 9, 7, 5 };
  mulib_price_demand_model_t model;
  assert (mulib_price_demand_fit (prices, units, 3, 0, &model));
  assert (mulib_price_demand_predict (&model, 4) == 3);
  return 0;
}
