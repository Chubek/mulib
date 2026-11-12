/* Complete, offline price-timeseries example. */
#define MULIB_IMPL_MONEY
#define MULIB_IMPL_PRICE_HISTORY
#define MULIB_IMPL_PRICE_OBSERVATION
#define MULIB_IMPL_PRICE_TIMESERIES
#include "mulib-price-timeseries.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_price_point_t points[]
      = { { 0, 10, 1, 1 }, { 5, 0, 0, 0 }, { 10, 20, 1, 1 } };
  assert (mulib_price_timeseries_interpolate (points, 3)
          && points[1].value == 15);
  return 0;
}
