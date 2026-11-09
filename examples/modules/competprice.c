#define MULIB_IMPL_STATSPROC
#define MULIB_IMPL_COMPETPRICE
#include "mulib-competprice.h"
#include <assert.h>

int main(void) {
  const mulib_competprice_rival_t rivals[]={{90,1,1,1},{100,3,1,1},{150,1,2,1}};
  mulib_competprice_config_t options={0};
  mulib_competprice_result_t result;
  options.policy=MULIB_COMPETPRICE_WEIGHTED_MEDIAN;
  options.relative_gap=-.05;options.min_price=80;options.max_price=200;
  assert(mulib_competprice_recommend(rivals,3,&options,&result));
  assert(result.price==95);
  return 0;
}
