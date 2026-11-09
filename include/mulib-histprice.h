/** @file mulib-histprice.h
 * @brief Univariate time-ordered price forecasts: EMA, Holt, seasonal Holt-
 * Winters, Kalman local level, AR(1), and robust Theil-Sen trend.
 * Define MULIB_IMPL_STATSPROC and MULIB_IMPL_HISTPRICE in one TU; link -lm.
 * Observations must be finite and nonnegative, equally spaced, oldest first.
 * Returned forecasts are mathematical extrapolations, not guaranteed prices.
 */
#ifndef MULIB_HISTPRICE_H
#define MULIB_HISTPRICE_H
#include <stddef.h>
#include "mulib-statsproc.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_histprice_forecast {
  double point;
  double variance; /* NAN unless model provides predictive observation variance */
} mulib_histprice_forecast_t;
/** EMA level at last timestamp; horizon>=1 (level stays constant). */
int mulib_histprice_ema(const double *prices, size_t n, double alpha,
    size_t horizon, mulib_histprice_forecast_t *out);
/** Damped Holt trend, alpha/beta in [0,1], phi in (0,1], horizon>=1. */
int mulib_histprice_holt(const double *prices, size_t n, double alpha,
    double beta, double phi, size_t horizon, mulib_histprice_forecast_t *out);
/** Additive Holt-Winters, complete >=2 seasonal cycles, length>=2.
 * alpha,beta,gamma in [0,1]; predicts seasonally repeating pattern. */
int mulib_histprice_seasonal(const double *prices, size_t n, size_t season,
    double alpha, double beta, double gamma, size_t horizon,
    mulib_histprice_forecast_t *out);
/** Scalar local-level Kalman filter, q=process variance, r=measurement
 * variance; predictive variance includes observation noise. */
int mulib_histprice_kalman(const double *prices, size_t n, double q,
    double r, size_t horizon, mulib_histprice_forecast_t *out);
/** Stationary AR(1) regression x_t=a+b*x_(t-1); |b|<1 required. */
int mulib_histprice_ar1(const double *prices, size_t n, size_t horizon,
    mulib_histprice_forecast_t *out);
/** Robust Theil-Sen trend, O(n^2), max 1024 observations. */
int mulib_histprice_robust_trend(const double *prices, size_t n,
    size_t horizon, mulib_histprice_forecast_t *out);
/** Out-of-sample one-step EMA MAE, uses prices[0] as first level. */
int mulib_histprice_ema_mae(const double *prices, size_t n, double alpha, double *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HISTPRICE) && !defined(MULIB_HISTPRICE_IMPLEMENTED)
#define MULIB_HISTPRICE_IMPLEMENTED
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_histprice_valid_(const double *p,size_t n,size_t horizon) {
  size_t i; if(!p||!n||!horizon||horizon>1000000) return 0;
  for(i=0;i<n;++i) if(!isfinite(p[i])||p[i]<0) return 0;
  return 1;
}
static int mulib_histprice_unit_(double x) {return isfinite(x)&&x>=0&&x<=1;}
static int mulib_histprice_publish_(double point,double var,mulib_histprice_forecast_t *out) {
  mulib_histprice_forecast_t r;
  if (!out||!isfinite(point)||point<0||(!isnan(var)&&(!isfinite(var)||var<0))) return 0;
  r.point=point;r.variance=var;*out=r;return 1;
}
int mulib_histprice_ema(const double *p,size_t n,double a,size_t h,mulib_histprice_forecast_t *out) {
  double level;size_t i;
  if(!mulib_histprice_valid_(p,n,h)||!mulib_histprice_unit_(a))return 0;
  level=p[0];for(i=1;i<n;++i)level=a*p[i]+(1-a)*level;
  return mulib_histprice_publish_(level,NAN,out);
}
int mulib_histprice_holt(const double *p,size_t n,double a,double b,double phi,size_t h,mulib_histprice_forecast_t *out) {
  double l,t,old;long double drift=0;size_t i;
  if(!mulib_histprice_valid_(p,n,h)||n<2||!mulib_histprice_unit_(a)||!mulib_histprice_unit_(b)||
     !isfinite(phi)||phi<=0||phi>1)return 0;
  l=p[0];t=p[1]-p[0];
  for(i=1;i<n;++i){old=l;l=a*p[i]+(1-a)*(l+phi*t);t=b*(l-old)+(1-b)*phi*t;}
  if(phi==1)drift=(long double)t*h;
  else drift=(long double)t*phi*(1-powl(phi,(long double)h))/(1-phi);
  return mulib_histprice_publish_((double)(l+drift),NAN,out);
}
int mulib_histprice_seasonal(const double *p,size_t n,size_t s,double a,double b,double g,size_t h,mulib_histprice_forecast_t *out) {
  double *seasonal, level,trend,mean0=0,mean1=0,old,fit;size_t i;
  if (!mulib_histprice_valid_(p,n,h)||s<2||s>n/2||!mulib_histprice_unit_(a)||
      !mulib_histprice_unit_(b)||!mulib_histprice_unit_(g)||s>SIZE_MAX/sizeof(double))return 0;
  seasonal=(double *)malloc(s*sizeof(*seasonal));if(!seasonal)return 0;
  for(i=0;i<s;++i){mean0+=p[i]/(double)s;mean1+=p[i+s]/(double)s;}
  level=mean0;trend=(mean1-mean0)/(double)s;
  for(i=0;i<s;++i)seasonal[i]=p[i]-mean0;
  for(i=s;i<n;++i){size_t j=i%s;old=level;
    level=a*(p[i]-seasonal[j])+(1-a)*(level+trend);
    trend=b*(level-old)+(1-b)*trend;
    seasonal[j]=g*(p[i]-level)+(1-g)*seasonal[j];
  }
  fit=(double)((long double)level+(long double)h*trend+seasonal[((n%s)+((h-1)%s))%s]);
  free(seasonal);return mulib_histprice_publish_(fit,NAN,out);
}
int mulib_histprice_kalman(const double *p,size_t n,double q,double r,size_t h,mulib_histprice_forecast_t *out) {
  long double x,v,k,den;size_t i;
  if(!mulib_histprice_valid_(p,n,h)||!isfinite(q)||q<0||!isfinite(r)||r<0||(q==0&&r==0))return 0;
  x=p[0];v=r;
  for(i=1;i<n;++i){v+=q;den=v+r;
    if(!isfinite(den)||den<=0)return 0;
    k=v/den;x+=k*(p[i]-x);v=r*k;
  }
  return mulib_histprice_publish_((double)x,(double)(v+(long double)h*q+r),out);
}
int mulib_histprice_ar1(const double *p,size_t n,size_t h,mulib_histprice_forecast_t *out) {
  long double mx=0,my=0,sxx=0,sxy=0;double a,b,pred;size_t i;
  if(!mulib_histprice_valid_(p,n,h)||n<3)return 0;
  for(i=1;i<n;++i){mx+=p[i-1];my+=p[i];}
  mx/=n-1;my/=n-1;
  for(i=1;i<n;++i){long double x=p[i-1]-mx,y=p[i]-my;sxx+=x*x;sxy+=x*y;}
  if(sxx==0)return 0;
  b=(double)(sxy/sxx);a=(double)(my-b*mx);
  if(!isfinite(a)||!isfinite(b)||fabs(b)>=1)return 0;
  pred=(double)(a*(1-pow(b,(double)h))/(1-b)+pow(b,(double)h)*p[n-1]);
  return mulib_histprice_publish_(pred,NAN,out);
}
int mulib_histprice_robust_trend(const double *p,size_t n,size_t h,mulib_histprice_forecast_t *out) {
  double x[1024],slope,intercept,pred;size_t i;
  if(!mulib_histprice_valid_(p,n,h)||n<2||n>1024)return 0;
  for(i=0;i<n;++i)x[i]=(double)i;
  if(!mulib_statsproc_theil_sen(x,p,n,&slope,&intercept))return 0;
  pred=(double)((long double)intercept+(long double)slope*(n-1+(long double)h));
  return mulib_histprice_publish_(pred,NAN,out);
}
int mulib_histprice_ema_mae(const double *p,size_t n,double a,double *out) {
  double l,result;long double sum=0;size_t i;
  if(!out||!mulib_histprice_valid_(p,n,1)||n<2||!mulib_histprice_unit_(a))return 0;
  l=p[0];for(i=1;i<n;++i){sum+=fabs(p[i]-l);l=a*p[i]+(1-a)*l;}
  result=(double)(sum/(n-1));if(!isfinite(result))return 0;*out=result;return 1;
}
#ifdef __cplusplus
}
#endif
#endif
