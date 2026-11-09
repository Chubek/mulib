/* Metamorphic invariants on market estimators and price optimizer. */
#define MULIB_IMPL_STATSPROC
#define MULIB_IMPL_STATSPRICE
#define MULIB_IMPL_PRICEOPT
#include "mulib-statsprice.h"
#include "mulib-priceopt.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static void near(double a,double b){assert(isfinite(a)&&fabs(a-b)<1e-7*fmax(1.0,fabs(b)));}
int main(void) {
  const double vals[]={10,15,21,22,100};double shifted[5],scaled[5],med,m2;
  size_t i;mulib_priceopt_constraints_t c={0,100,10,0,0,0,0};
  mulib_priceopt_demand_model_t m={MULIB_PRICEOPT_LINEAR,100,1,0};
  mulib_priceopt_result_t r;
  assert(mulib_statsprice_estimate(vals,5,MULIB_STATSPRICE_MEDIAN,0,&med));
  for(i=0;i<5;++i){shifted[i]=vals[i]+100;scaled[i]=vals[i]*2;}
  assert(mulib_statsprice_estimate(shifted,5,MULIB_STATSPRICE_MEDIAN,0,&m2));near(m2,med+100);
  assert(mulib_statsprice_estimate(scaled,5,MULIB_STATSPRICE_MEDIAN,0,&m2));near(m2,med*2);
  for(i=1;i<100;i+=3){c.min_price=(double)i/2;c.max_price=(double)i;
    if(c.max_price<c.unit_cost)continue;
    assert(mulib_priceopt_optimize(&m,&c,MULIB_PRICEOPT_PROFIT,&r));
    assert(r.price>=c.min_price&&r.price<=c.max_price);
    assert(r.price+1e-8>=c.unit_cost);
  }
  puts("pricing metamorphic properties passed");return 0;
}
