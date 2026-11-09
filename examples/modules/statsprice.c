#define MULIB_IMPL_STATSPROC
#define MULIB_IMPL_STATSPRICE
#include "mulib-statsprice.h"
#include <assert.h>

int main(void) {
  const double prices[]={90,95,100,110,150};
  mulib_statsprice_summary_t summary;
  assert(mulib_statsprice_summarize(prices,5,&summary));
  assert(summary.median==100&&summary.mad==10);
  return 0;
}
