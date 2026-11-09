#define MULIB_IMPL_PRICEOPT
#include "mulib-priceopt.h"
#include <assert.h>

int main(void) {
  const mulib_priceopt_demand_model_t model={MULIB_PRICEOPT_LINEAR,100,1,0};
  const mulib_priceopt_constraints_t constraints={0,100,20,0,0,0,5};
  mulib_priceopt_result_t result;
  assert(mulib_priceopt_optimize(&model,&constraints,MULIB_PRICEOPT_PROFIT,&result));
  assert(result.price==60&&result.score==1600);
  return 0;
}
