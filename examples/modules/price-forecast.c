/* Complete, offline price-forecast example. */
#define MULIB_IMPL_MONEY
#define MULIB_IMPL_PRICE_FORECAST
#define MULIB_IMPL_PRICE_HISTORY
#define MULIB_IMPL_PRICE_OBSERVATION
#define MULIB_IMPL_PRICE_TIMESERIES
#include "mulib-price-forecast.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  double series[] = { 10, 20, 30 }, prediction;
  assert (mulib_price_forecast (series, 3, 2, MULIB_PRICE_FORECAST_DRIFT, 0, 0,
                                &prediction));
  assert (prediction == 50);
  return 0;
}
