/** @file mulib-statsprice.h
 * @brief Robust cross-sectional market price estimators. Requires statsproc.
 * Define MULIB_IMPL_STATSPROC and MULIB_IMPL_STATSPRICE in one C11 TU; -lm.
 */
#ifndef MULIB_STATSPRICE_H
#define MULIB_STATSPRICE_H
#include <stddef.h>
#include "mulib-statsproc.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_statsprice_method {
  MULIB_STATSPRICE_MEDIAN = 0,
  MULIB_STATSPRICE_TRIMMED = 1,
  MULIB_STATSPRICE_WINSORIZED = 2,
  MULIB_STATSPRICE_HUBER = 3,
  MULIB_STATSPRICE_QUANTILE = 4,
  MULIB_STATSPRICE_KDE_MODE = 5
} mulib_statsprice_method_t;
typedef struct mulib_statsprice_summary {
  double median, mean, mad, q25, q75, huber;
} mulib_statsprice_summary_t;
/** All prices must be finite and >=0. Parameter: trim fraction for trimmed /
 * winsorized, quantile p for QUANTILE; ignored for MEDIAN and HUBER; positive
 * bandwidth for KDE (0 uses a robust automatic bandwidth). */
int mulib_statsprice_estimate(const double *prices, size_t n,
    mulib_statsprice_method_t method, double parameter, double *out);
/** Weights >=0, total >0. If robust=1, discard samples more than 3*MAD
 * above/below median; if MAD=0 no samples are filtered. */
int mulib_statsprice_weighted(const double *prices, const double *weights,
    size_t n, int robust, double *out);
/** Market snapshot, no assumptions about time, sales, or demand. */
int mulib_statsprice_summarize(const double *prices, size_t n,
    mulib_statsprice_summary_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_STATSPRICE) && !defined(MULIB_STATSPRICE_IMPLEMENTED)
#define MULIB_STATSPRICE_IMPLEMENTED
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_statsprice_validate_(const double *p, size_t n) {
  size_t i; if (!p || !n) return 0;
  for (i=0;i<n;++i) if (!isfinite(p[i]) || p[i]<0) return 0;
  return 1;
}
int mulib_statsprice_estimate(const double *p, size_t n, mulib_statsprice_method_t m, double parameter, double *out) {
  double x, lo,hi,mean=0,sd=0,h;
  size_t i;
  if (!out || !mulib_statsprice_validate_(p,n)) return 0;
  switch(m) {
  case MULIB_STATSPRICE_MEDIAN: return mulib_statsproc_quantile(p,n,0.5,out);
  case MULIB_STATSPRICE_TRIMMED: return mulib_statsproc_trimmed_mean(p,n,parameter,out);
  case MULIB_STATSPRICE_WINSORIZED: return mulib_statsproc_winsor_mean(p,n,parameter,out);
  case MULIB_STATSPRICE_HUBER: return mulib_statsproc_huber(p,n,1.345,50,out);
  case MULIB_STATSPRICE_QUANTILE: return mulib_statsproc_quantile(p,n,parameter,out);
  case MULIB_STATSPRICE_KDE_MODE:
    if (!isfinite(parameter) || parameter<0) return 0;
    h=parameter;
    if (h==0) {
      long double ss=0, avg=0;
      if (!mulib_statsproc_quantile(p,n,0.25,&lo) || !mulib_statsproc_quantile(p,n,0.75,&hi)) return 0;
      for (i=0;i<n;++i) avg+=p[i];
      avg/=n;
      for (i=0;i<n;++i) {long double d=p[i]-avg;ss+=d*d;}
      mean=(double)avg;sd=n>1?(double)sqrtl(ss/(n-1)):0;
      x=fmin(sd,(hi-lo)/1.34);
      if (x<=0) x=sd;
      if (x<=0) x=fmax(1.0,mean)*0.01;
      h=0.9*x*pow((double)n,-0.2);
      if (!isfinite(h) || h<=0) return 0;
    }
    return mulib_statsproc_kde_mode(p,n,h,256,out);
  default: return 0;
  }
}
int mulib_statsprice_weighted(const double *p, const double *w, size_t n, int robust, double *out) {
  double med,mad,result; double *filtered; size_t i;
  if (!out || !w || !mulib_statsprice_validate_(p,n) || (robust!=0&&robust!=1)) return 0;
  if (!robust) return mulib_statsproc_weighted_mean(p,w,n,out);
  if (n>SIZE_MAX/sizeof(double) || !mulib_statsproc_quantile(p,n,0.5,&med) || !mulib_statsproc_mad(p,n,&mad)) return 0;
  for (i=0;i<n;++i) if (!isfinite(w[i]) || w[i]<0) return 0;
  filtered=(double *)malloc(n*sizeof(double)); if (!filtered) return 0;
  for (i=0;i<n;++i) filtered[i]=(mad>0&&fabs(p[i]-med)>3*mad)?0:w[i];
  i=mulib_statsproc_weighted_mean(p,filtered,n,&result);free(filtered);
  if (!i) return 0;
  *out=result; return 1;
}
int mulib_statsprice_summarize(const double *p, size_t n, mulib_statsprice_summary_t *out) {
  mulib_statsprice_summary_t r; long double sum=0; size_t i;
  if (!out || !mulib_statsprice_validate_(p,n)) return 0;
  for(i=0;i<n;++i) sum+=p[i];
  r.mean=(double)(sum/n);
  if (!isfinite(r.mean) || !mulib_statsproc_quantile(p,n,0.5,&r.median) ||
      !mulib_statsproc_quantile(p,n,0.25,&r.q25) ||
      !mulib_statsproc_quantile(p,n,0.75,&r.q75) ||
      !mulib_statsproc_mad(p,n,&r.mad) ||
      !mulib_statsproc_huber(p,n,1.345,50,&r.huber)) return 0;
  *out=r;return 1;
}
#ifdef __cplusplus
}
#endif
#endif
