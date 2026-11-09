#define MULIB_IMPL_STATSPROC
#define MULIB_IMPL_HISTPRICE
#include "mulib-histprice.h"
#include <assert.h>

int main(void) {
  const double prices[]={10,20,30,40,50};
  mulib_histprice_forecast_t forecast;
  assert(mulib_histprice_holt(prices,5,1,1,1,2,&forecast));
  assert(forecast.point==70);
  return 0;
}
