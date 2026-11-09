#define MULIB_IMPL_STATSPROC
#include "mulib-statsproc.h"
#include <assert.h>

int main(void) {
  const double samples[]={90,100,100,110,900};
  double median,huber;
  assert(mulib_statsproc_quantile(samples,5,.5,&median));
  assert(mulib_statsproc_huber(samples,5,1.345,50,&huber));
  assert(median==100&&huber>100&&huber<110);
  return 0;
}
