/* Complete, offline price-ensemble example. */
#define MULIB_IMPL_PRICE_ENSEMBLE
#include "mulib-price-ensemble.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  double forecasts[] = { 10, 20 }, errors[] = { 1, 1 }, mean, spread;
  assert (mulib_price_ensemble_blend (forecasts, errors, 2, &mean, &spread));
  assert (mean == 15 && spread == 5);
  return 0;
}
