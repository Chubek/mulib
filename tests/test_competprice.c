#define MULIB_IMPL_STATSPROC
#define MULIB_IMPL_COMPETPRICE
#include "mulib-competprice.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <float.h>
static void near(double x,double y){assert(fabs(x-y)<1e-8);}
int main(void) {
  mulib_competprice_rival_t r[]={{90,1,1,1},{100,3,1,1},{150,1,2,1}};
  mulib_competprice_config_t c={0};mulib_competprice_result_t out;
  c.min_price=80;c.max_price=200;c.policy=MULIB_COMPETPRICE_WEIGHTED_MEDIAN;
  c.relative_gap=-0.05;
  assert(mulib_competprice_recommend(r,3,&c,&out));near(out.benchmark,100);near(out.price,95);
  c.policy=MULIB_COMPETPRICE_LOWEST; c.absolute_gap=5;
  assert(mulib_competprice_recommend(r,3,&c,&out));near(out.price,80.5);
  c.policy=MULIB_COMPETPRICE_LEADER;c.relative_gap=0;c.absolute_gap=0;
  assert(mulib_competprice_recommend(r,3,&c,&out));near(out.price,100);
  c.policy=MULIB_COMPETPRICE_WEIGHTED_MEAN;
  assert(mulib_competprice_recommend(r,3,&c,&out));near(out.price,108);
  c.policy=MULIB_COMPETPRICE_QUANTILE;c.percentile=0.5;
  assert(mulib_competprice_recommend(r,3,&c,&out));near(out.price,100);
  c.policy=MULIB_COMPETPRICE_QUALITY_ADJUSTED;c.own_quality=2;c.quality_exponent=1;
  assert(mulib_competprice_recommend(r,3,&c,&out));near(out.benchmark,200);
  c.max_price=140;assert(mulib_competprice_recommend(r,3,&c,&out));near(out.price,140);assert(out.clamped);
  c.previous_price=100;c.max_relative_change=.1;
  assert(mulib_competprice_recommend(r,3,&c,&out));near(out.price,110);
  c.min_price=115;assert(!mulib_competprice_recommend(r,3,&c,&out));
  c.min_price=80;c.max_relative_change=0;c.previous_price=0;c.percentile=NAN;c.policy=MULIB_COMPETPRICE_QUANTILE;
  out.price=1234;assert(!mulib_competprice_recommend(r,3,&c,&out)&&out.price==1234);
  c.percentile=.5;r[0].price=-1;
  assert(!mulib_competprice_recommend(r,3,&c,&out));
  {mulib_competprice_rival_t one={1,1,DBL_MIN,1};
    mulib_competprice_config_t quality={0};
    quality.policy=MULIB_COMPETPRICE_QUALITY_ADJUSTED;
    quality.own_quality=DBL_MAX;quality.max_price=10;
    assert(mulib_competprice_recommend(&one,1,&quality,&out));near(out.price,1);
    one.active=0;out.price=1234;
    assert(!mulib_competprice_recommend(&one,1,&quality,&out)&&out.price==1234);
  }
  puts("competprice passed");return 0;
}
