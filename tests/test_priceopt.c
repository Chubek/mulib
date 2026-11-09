#define MULIB_IMPL_PRICEOPT
#include "mulib-priceopt.h"
#include <assert.h>
#include <math.h>
#include <float.h>
#include <stdio.h>
static void near(double x,double y){assert(isfinite(x)&&fabs(x-y)<1e-6*fmax(1.0,fabs(y)));}
int main(void) {
  mulib_priceopt_demand_model_t linear={MULIB_PRICEOPT_LINEAR,100,1,0};
  mulib_priceopt_demand_model_t elastic={MULIB_PRICEOPT_ISOELASTIC,100,2,50};
  mulib_priceopt_demand_model_t logistic={MULIB_PRICEOPT_LOGISTIC,100,10,50};
  mulib_priceopt_constraints_t c={0,100,20,0,0,0,0};
  mulib_priceopt_result_t r;double p[]={30,50,70,90},q[]={70,50,30,10};size_t i;
  near(mulib_priceopt_demand(&linear,25),75);
  near(mulib_priceopt_demand(&elastic,50),100);
  near(mulib_priceopt_demand(&logistic,50),50);
  assert(mulib_priceopt_optimize(&linear,&c,MULIB_PRICEOPT_PROFIT,&r));near(r.price,60);near(r.score,1600);
  assert(mulib_priceopt_optimize(&linear,&c,MULIB_PRICEOPT_REVENUE,&r));near(r.price,50);
  c.tick=5;assert(mulib_priceopt_optimize(&linear,&c,MULIB_PRICEOPT_PROFIT,&r));near(r.price,60);
  c.tick=0;c.min_unit_margin=35;
  assert(mulib_priceopt_optimize(&linear,&c,MULIB_PRICEOPT_PROFIT,&r));near(r.price,60);
  c.previous_price=40;c.max_relative_change=.1;
  assert(!mulib_priceopt_optimize(&linear,&c,MULIB_PRICEOPT_PROFIT,&r)); /* margin floor > move cap */
  c.previous_price=0;c.max_relative_change=0;c.min_unit_margin=0;
  assert(mulib_priceopt_candidates(p,q,4,&c,MULIB_PRICEOPT_PROFIT,&r));near(r.price,50);
  {const mulib_priceopt_demand_model_t models[]={linear,{MULIB_PRICEOPT_LINEAR,120,1,0}};
    const double weights[]={1,1};
    assert(mulib_priceopt_scenarios(models,weights,2,0,&c,MULIB_PRICEOPT_PROFIT,&r));near(r.price,65);
    assert(mulib_priceopt_scenarios(models,weights,2,1,&c,MULIB_PRICEOPT_PROFIT,&r));
    assert(r.price>=20&&r.price<=100);
  }
  assert(mulib_priceopt_optimize(&elastic,&c,MULIB_PRICEOPT_PROFIT,&r));assert(r.price>20&&r.price<100);
  assert(mulib_priceopt_optimize(&logistic,&c,MULIB_PRICEOPT_PROFIT,&r));assert(r.price>20&&r.price<100);
  {size_t trials[]={10,0,10};double reward[]={.2,0,.5};
    assert(mulib_priceopt_ucb(trials,reward,3,2,&i)&&i==1);
    trials[1]=10;assert(mulib_priceopt_ucb(trials,reward,3,0,&i)&&i==2);
  }
  r.price=1234;c.max_price=-1;
  assert(!mulib_priceopt_optimize(&linear,&c,MULIB_PRICEOPT_PROFIT,&r)&&r.price==1234);
  assert(isnan(mulib_priceopt_demand(&elastic,0)));
  {mulib_priceopt_demand_model_t constant={MULIB_PRICEOPT_LINEAR,1,0,0};
    mulib_priceopt_constraints_t ticks={.3,.3,0,0,0,0,.1};
    assert(mulib_priceopt_optimize(&constant,&ticks,MULIB_PRICEOPT_REVENUE,&r));
    assert(r.price==.3&&r.evaluations==1);
    assert(mulib_priceopt_candidates((double[]){.3},(double[]){1},1,&ticks,MULIB_PRICEOPT_REVENUE,&r));
    ticks.min_price=.31;ticks.max_price=.39;r.price=1234;
    assert(!mulib_priceopt_optimize(&constant,&ticks,MULIB_PRICEOPT_REVENUE,&r)&&r.price==1234);
    ticks.min_price=1;ticks.max_price=1;ticks.tick=DBL_MIN;
    assert(!mulib_priceopt_candidates((double[]){1},(double[]){1},1,&ticks,MULIB_PRICEOPT_REVENUE,&r));
  }
  {mulib_priceopt_demand_model_t huge={MULIB_PRICEOPT_ISOELASTIC,1e300,1,1e-300};
    near(mulib_priceopt_demand(&huge,1),1);
    huge.scale=0;assert(mulib_priceopt_demand(&huge,DBL_MIN)==0);
  }
  {const mulib_priceopt_demand_model_t models[]={{MULIB_PRICEOPT_LINEAR,10,0,0},elastic};
    const double weights[]={1,0};mulib_priceopt_constraints_t fixed={0,0,0,0,0,0,0};
    assert(mulib_priceopt_scenarios(models,weights,2,1,&fixed,MULIB_PRICEOPT_PROFIT,&r));
    assert(r.price==0&&r.score==0&&r.expected_demand==10);
    r.price=1234;
    assert(!mulib_priceopt_scenarios(models,(double[]){0,0},2,0,&fixed,MULIB_PRICEOPT_PROFIT,&r)&&r.price==1234);
  }
  {const mulib_priceopt_demand_model_t models[]={{MULIB_PRICEOPT_LINEAR,1e12,0,0},
      {MULIB_PRICEOPT_LINEAR,1e12+2,0,0}};
    mulib_priceopt_constraints_t fixed={1,1,0,0,0,0,0};
    assert(mulib_priceopt_scenarios(models,(double[]){1,1},2,1,&fixed,MULIB_PRICEOPT_PROFIT,&r));
    assert(fabs(r.score-1e12)<.001); /* stddev=1, not zero from cancellation */
  }
  puts("priceopt passed");return 0;
}
