#define MULIB_IMPL_STATSPROC
#define MULIB_IMPL_HISTPRICE
#include "mulib-histprice.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <float.h>
static void near(double x,double y){assert(isfinite(x)&&fabs(x-y)<1e-7*fmax(1.0,fabs(y)));}
int main(void) {
  const double flat[]={100,100,100,100,100,100,100,100};
  const double ramp[]={10,20,30,40,50};
  const double season[]={100,120,100,120,100,120,100,120};
  mulib_histprice_forecast_t f={0}; double mae;
  assert(mulib_histprice_ema(flat,8,.4,1,&f));near(f.point,100);
  assert(isnan(f.variance));
  assert(mulib_histprice_holt(flat,8,.5,.3,.9,4,&f));near(f.point,100);
  assert(mulib_histprice_holt(ramp,5,1,1,1,2,&f));near(f.point,70);
  assert(mulib_histprice_seasonal(season,8,2,0,0,0,1,&f));near(f.point,100);
  assert(mulib_histprice_seasonal(season,8,2,0,0,0,2,&f));near(f.point,120);
  assert(mulib_histprice_kalman(flat,8,1,1,3,&f));near(f.point,100);assert(f.variance>=3);
  assert(mulib_histprice_ar1(flat,8,1,&f)==0); /* zero lag variance */
  {const double decay[]={64,32,16,8,4,2,1};
    assert(mulib_histprice_ar1(decay,7,1,&f));near(f.point,.5);
  }
  assert(mulib_histprice_robust_trend(ramp,5,2,&f));near(f.point,70);
  assert(mulib_histprice_ema_mae(flat,8,.3,&mae));near(mae,0);
  f.point=222;assert(!mulib_histprice_ema(ramp,5,-1,1,&f)&&f.point==222);
  assert(!mulib_histprice_seasonal(flat,8,5,.3,.3,.3,1,&f));
  assert(!mulib_histprice_ar1(ramp,5,0,&f));
  {const double large[]={100,100};
    assert(mulib_histprice_kalman(large,2,0,DBL_MAX*.6,1,&f));
    near(f.point,100);assert(isfinite(f.variance)&&f.variance>DBL_MAX/2);
  }
  assert(mulib_histprice_kalman(flat,8,1,0,1,&f));
  near(f.point,100);near(f.variance,1);
  f.point=222;f.variance=333;
  assert(!mulib_histprice_kalman(flat,8,0,0,1,&f)&&f.point==222&&f.variance==333);
  assert(!mulib_histprice_ema(flat,8,.5,1000001,&f));
  puts("histprice passed");return 0;
}
