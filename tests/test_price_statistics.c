#define MULIB_IMPL_STATSPROC
#define MULIB_IMPL_STATSPRICE
#include "mulib-statsprice.h"
#include <assert.h>
#include <math.h>
#include <float.h>
#include <stdio.h>
static void near(double x,double y){assert(isfinite(x)&&fabs(x-y)<1e-8*fmax(1.0,fabs(y)));}
int main(void) {
  const double a[]={90,95,100,110,150};
  const double w[]={1,1,2,1,0};
  const double bad[]={1,NAN,3};
  double x=777,y,s,b;mulib_statsprice_summary_t r;
  near((assert(mulib_statsproc_quantile(a,5,0.5,&x)),x),100);
  assert(mulib_statsproc_quantile(a,5,0.25,&x));near(x,95);
  assert(mulib_statsproc_trimmed_mean(a,5,0.2,&x));near(x,305.0/3);
  assert(mulib_statsproc_winsor_mean(a,5,0.2,&x));near(x,102);
  assert(mulib_statsproc_weighted_mean(a,w,5,&x));near(x,99);
  assert(mulib_statsproc_weighted_quantile(a,w,5,0.5,&x));near(x,100);
  assert(mulib_statsproc_mad(a,5,&x));near(x,10);
  assert(mulib_statsproc_huber(a,5,1.345,100,&x));assert(x>95&&x<110);
  assert(mulib_statsproc_kde_mode(a,5,10,256,&x));assert(x>92&&x<108);
  { double xx[]={0,1,2,3,4}, yy[]={1,3,5,7,100};
    assert(mulib_statsproc_theil_sen(xx,yy,5,&s,&b));near(s,2);near(b,1);
  }
  assert(mulib_statsprice_summarize(a,5,&r));near(r.median,100);near(r.mad,10);
  assert(mulib_statsprice_estimate(a,5,MULIB_STATSPRICE_MEDIAN,0,&x));near(x,100);
  assert(mulib_statsprice_estimate(a,5,MULIB_STATSPRICE_HUBER,0,&x));assert(x<110);
  assert(mulib_statsprice_estimate(a,5,MULIB_STATSPRICE_KDE_MODE,0,&x));assert(x>90&&x<112);
  assert(mulib_statsprice_weighted(a,w,5,1,&x));assert(x>95&&x<110);
  y=1234;assert(!mulib_statsprice_estimate(bad,3,MULIB_STATSPRICE_MEDIAN,0,&y)&&y==1234);
  assert(!mulib_statsproc_quantile(a,5,1.1,&y)&&y==1234);
  assert(!mulib_statsproc_weighted_mean(a,NULL,5,&y)&&y==1234);
  assert(!mulib_statsproc_trimmed_mean(a,5,0.5,&y)&&y==1234);
  assert(!mulib_statsproc_theil_sen(NULL,a,5,&s,&b));
  {double neg[]={-1,5};assert(!mulib_statsprice_summarize(neg,2,&r));}
  {const double extremes[]={-DBL_MAX,DBL_MAX};
    assert(mulib_statsproc_quantile(extremes,2,0,&x)&&x==-DBL_MAX);
    assert(mulib_statsproc_quantile(extremes,2,.5,&x)&&x==0);
    assert(mulib_statsproc_quantile(extremes,2,1,&x)&&x==DBL_MAX);
    assert(mulib_statsproc_mad(extremes,2,&x)&&x==DBL_MAX);
    assert(mulib_statsproc_theil_sen(extremes,extremes,2,&s,&b));near(s,1);near(b,0);
    x=1234;assert(!mulib_statsproc_theil_sen(extremes,extremes,2,&x,&x)&&x==1234);
  }
  {const double large[]={0,1e200,2e200,3e200};
    assert(mulib_statsprice_estimate(large,4,MULIB_STATSPRICE_KDE_MODE,0,&x));
    assert(x>=0&&x<=3e200);
  }
  {const double masses[]={0,1,0},values[]={1,2,3};
    assert(mulib_statsproc_weighted_quantile(values,masses,3,0,&x)&&x==2);
    assert(mulib_statsproc_weighted_quantile(values,masses,3,1,&x)&&x==2);
    assert(mulib_statsproc_weighted_quantile(values,(double[]){DBL_MAX,DBL_MIN,0},3,1,&x)&&x==2);
    y=1234;assert(!mulib_statsproc_weighted_mean(values,(double[]){0,0,0},3,&y)&&y==1234);
  }
  puts("statsproc/statsprice passed");return 0;
}
