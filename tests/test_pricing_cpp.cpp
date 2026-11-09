#include "mulib-statsprice.h"
#include "mulib-competprice.h"
#include "mulib-histprice.h"
#include "mulib-priceopt.h"
#include "mulib-sat.h"
#include "mulib-smt.h"
#include <cassert>
int main() {
  const double p[]{20,30,40};double m=0;
  assert(mulib_statsprice_estimate(p,3,MULIB_STATSPRICE_MEDIAN,0,&m));assert(m==30);
  assert(mulib_statsproc_quantile(p,3,.5,&m)&&m==30);
  mulib_histprice_forecast_t forecast{};
  assert(mulib_histprice_ema(p,3,1,1,&forecast)&&forecast.point==40);
  const mulib_priceopt_demand_model_t demand{MULIB_PRICEOPT_LINEAR,100,1,0};
  const mulib_priceopt_constraints_t limits{0,100,20,0,0,0,5};
  mulib_priceopt_result_t optimum{};
  assert(mulib_priceopt_optimize(&demand,&limits,MULIB_PRICEOPT_PROFIT,&optimum));
  assert(optimum.price==60);
  const mulib_competprice_rival_t rival{100,1,1,1};
  mulib_competprice_config_t options{};options.max_price=200;
  mulib_competprice_result_t quote{};
  assert(mulib_competprice_recommend(&rival,1,&options,&quote)&&quote.price==100);
  mulib_sat_t *sat=mulib_sat_create(1,0);
  assert(sat&&mulib_sat_add_clause(sat,nullptr,0));
  assert(mulib_sat_solve(sat,nullptr,0)==MULIB_SAT_UNSAT);
  mulib_sat_destroy(sat);
  mulib_smt_t *smt=mulib_smt_create(0,2,0);double x,y;
  assert(smt&&mulib_smt_difference(smt,0,1,-3,0));
  assert(mulib_smt_solve(smt)==MULIB_SMT_SAT);
  assert(mulib_smt_real_value(smt,0,&x)&&mulib_smt_real_value(smt,1,&y)&&x-y<=-3);
  mulib_smt_destroy(smt);
}
