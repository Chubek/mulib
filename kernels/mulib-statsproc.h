/** @file mulib-statsproc.h
 * @brief Robust, dependency-free statistical primitives for price estimation.
 * Define MULIB_IMPL_STATSPROC in exactly one translation unit; link -lm.
 * All inputs are borrowed. Functions return 1 on success and preserve outputs
 * on failure. Empty/nonfinite samples and invalid weights are rejected.
 * Generic statistics accept signed values; price wrappers reject negatives.
 */
#ifndef MULIB_STATSPROC_H
#define MULIB_STATSPROC_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Quantile using Hyndman-Fan type 7 interpolation, p in [0,1]. */
int mulib_statsproc_quantile(const double *values, size_t n, double p, double *out);
/** A symmetric trimmed mean; trim in [0,0.5), floor(n*trim) at either end. */
int mulib_statsproc_trimmed_mean(const double *values, size_t n, double trim, double *out);
/** Symmetric winsorized mean; replace tails at the corresponding order stats. */
int mulib_statsproc_winsor_mean(const double *values, size_t n, double trim, double *out);
/** Weighted arithmetic mean, strictly positive total weight. Zero weights permitted. */
int mulib_statsproc_weighted_mean(const double *values, const double *weights, size_t n, double *out);
/** Weighted nearest-CDF quantile, p in [0,1]. No interpolation within masses. */
int mulib_statsproc_weighted_quantile(const double *values, const double *weights, size_t n, double p, double *out);
/** Median absolute deviation (unscaled), defined about the sample median. */
int mulib_statsproc_mad(const double *values, size_t n, double *out);
/** Huber location IRLS with k>0; k=1.345 is a standard default.
 * At most max_iter iterations (0 selects 50), reltol=1e-9. */
int mulib_statsproc_huber(const double *values, size_t n, double k, size_t max_iter, double *out);
/** Gaussian kernel density mode on an inclusive uniform grid (2..4096 points).
 * Bandwidth >0; grid discretization is deliberate. */
int mulib_statsproc_kde_mode(const double *values, size_t n, double bandwidth, size_t grid, double *out);
/** Robust line y=intercept+slope*x via median pairwise slope (Theil-Sen).
 * Requires 2..1024 rows, with >=1 unequal x pair; O(n*n log n) time.
 * Slope and intercept outputs must be distinct. */
int mulib_statsproc_theil_sen(const double *x, const double *y, size_t n, double *slope, double *intercept);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_STATSPROC_H */
#if defined(MULIB_IMPL_STATSPROC) && !defined(MULIB_STATSPROC_IMPLEMENTED)
#define MULIB_STATSPROC_IMPLEMENTED
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_statsproc_cmp_(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b;
  return (x > y) - (x < y);
}
static int mulib_statsproc_valid_(const double *a, size_t n, int price) {
  size_t i; if (!a || !n) return 0;
  for (i = 0; i < n; ++i) if (!isfinite(a[i]) || (price && a[i] < 0)) return 0;
  return 1;
}
static double *mulib_statsproc_sorted_(const double *a, size_t n, int price) {
  double *b;
  if (n > SIZE_MAX/sizeof(double) || !mulib_statsproc_valid_(a,n,price)) return NULL;
  b=(double *)malloc(n*sizeof(*b));
  if (b) { memcpy(b,a,n*sizeof(*b)); qsort(b,n,sizeof(*b),mulib_statsproc_cmp_); }
  return b;
}
static double mulib_statsproc_qsorted_(const double *a, size_t n, double p) {
  double t = p * (double)(n - 1); size_t i = (size_t)t;
  double f=t-(double)i;
  if (i+1>=n || f==0) return a[i];
  /* Opposite-sign endpoints can overflow when subtracted, even at p=0. */
  return a[i]<0 && a[i+1]>0 ? (1-f)*a[i]+f*a[i+1]
                           : a[i]+(a[i+1]-a[i])*f;
}
int mulib_statsproc_quantile(const double *a, size_t n, double p, double *out) {
  double *b, result;
  if (!out || !isfinite(p) || p<0 || p>1 || !(b=mulib_statsproc_sorted_(a,n,0))) return 0;
  result=mulib_statsproc_qsorted_(b,n,p); free(b);
  if (!isfinite(result)) return 0;
  *out=result; return 1;
}
int mulib_statsproc_trimmed_mean(const double *a, size_t n, double trim, double *out) {
  double *b; size_t i, t; long double sum=0; double result;
  if (!out || !isfinite(trim) || trim<0 || trim>=0.5 || !(b=mulib_statsproc_sorted_(a,n,0))) return 0;
  t=(size_t)(n*trim); for (i=t;i<n-t;++i) sum += b[i];
  result=(double)(sum/(n-2*t)); free(b);
  if (!isfinite(result)) return 0;
  *out=result; return 1;
}
int mulib_statsproc_winsor_mean(const double *a, size_t n, double trim, double *out) {
  double *b; size_t i,t; long double sum=0; double result;
  if (!out || !isfinite(trim) || trim<0 || trim>=0.5 || !(b=mulib_statsproc_sorted_(a,n,0))) return 0;
  t=(size_t)(n*trim);
  for (i=0;i<n;++i) sum += b[i<t?t:(i>=n-t?n-t-1:i)];
  result=(double)(sum/n); free(b);
  if (!isfinite(result)) return 0;
  *out=result; return 1;
}
int mulib_statsproc_weighted_mean(const double *a, const double *w, size_t n, double *out) {
  size_t i; long double sum=0,total=0; double result;
  if (!out || !w || !mulib_statsproc_valid_(a,n,0)) return 0;
  for (i=0;i<n;++i) {
    if (!isfinite(w[i]) || w[i]<0) return 0;
    total += (long double)w[i]; sum += (long double)w[i]*a[i];
  }
  if (total<=0) return 0;
  result=(double)(sum/total);
  if (!isfinite(result)) return 0;
  *out=result; return 1;
}
typedef struct mulib_statsproc_pair_ { double v,w; } mulib_statsproc_pair_;
static int mulib_statsproc_paircmp_(const void *x, const void *y) {
  const mulib_statsproc_pair_ *a=(const mulib_statsproc_pair_ *)x, *b=(const mulib_statsproc_pair_ *)y;
  return (a->v>b->v)-(a->v<b->v);
}
int mulib_statsproc_weighted_quantile(const double *a, const double *w, size_t n, double p, double *out) {
  mulib_statsproc_pair_ *b; size_t i; long double total=0,cum=0; double result;
  if (!out || !w || !mulib_statsproc_valid_(a,n,0) || !isfinite(p) || p<0 || p>1 || n>SIZE_MAX/sizeof(*b)) return 0;
  b=(mulib_statsproc_pair_ *)malloc(n*sizeof(*b)); if (!b) return 0;
  for (i=0;i<n;++i) {
    if (!isfinite(w[i]) || w[i]<0) { free(b); return 0; }
    b[i].v=a[i]; b[i].w=w[i]; total+=(long double)w[i];
  }
  if (!(total>0) || !isfinite(total)) { free(b); return 0; }
  qsort(b,n,sizeof(*b),mulib_statsproc_paircmp_);
  /* p=1 is the greatest positive-mass value, even if a tiny final weight
   * disappears when accumulated alongside much larger weights. */
  if (p==1) {
    for (i=n;i>0;--i) if (b[i-1].w>0) {
      result=b[i-1].v;free(b);*out=result;return 1;
    }
  }
  result=b[n-1].v;
  for (i=0;i<n;++i) if (b[i].w>0) {
    cum+=(long double)b[i].w;
    if (p==0 || cum/total >= p) { result=b[i].v; break; }
  }
  free(b); *out=result; return 1;
}
int mulib_statsproc_mad(const double *a, size_t n, double *out) {
  double *b, med, val; size_t i;
  if (!out || !(b=mulib_statsproc_sorted_(a,n,0))) return 0;
  med=mulib_statsproc_qsorted_(b,n,0.5);
  for (i=0;i<n;++i) b[i]=fabs(a[i]-med);
  qsort(b,n,sizeof(*b),mulib_statsproc_cmp_); val=mulib_statsproc_qsorted_(b,n,0.5);
  free(b); if (!isfinite(val)) return 0; *out=val; return 1;
}
int mulib_statsproc_huber(const double *a, size_t n, double k, size_t max_iter, double *out) {
  size_t i,iter; double center, mad, scale, result;
  if (!out || !isfinite(k) || k<=0 || !mulib_statsproc_quantile(a,n,0.5,&center) || !mulib_statsproc_mad(a,n,&mad)) return 0;
  scale=mad*1.482602218505602; if (scale==0) { *out=center; return 1; }
  if (!isfinite(scale)) return 0;
  if (max_iter==0) max_iter=50;
  for (iter=0;iter<max_iter;++iter) {
    long double num=0,den=0;
    for (i=0;i<n;++i) {
      long double d=fabsl(((long double)a[i]-center)/scale);
      long double w=d<=k?1:(long double)k/d;
      num+=w*a[i]; den+=w;
    }
    result=(double)(num/den); if (!isfinite(result)) return 0;
    if (fabs(result-center)<=1e-9*fmax(1.0,fabs(center))) { center=result; break; }
    center=result;
  }
  *out=center; return 1;
}
int mulib_statsproc_kde_mode(const double *a, size_t n, double h, size_t grid, double *out) {
  size_t i,j; double lo,hi,mode, best=-1;
  if (!out || !mulib_statsproc_valid_(a,n,0) || !isfinite(h) || h<=0 || grid<2 || grid>4096) return 0;
  lo=hi=a[0]; for (i=1;i<n;++i) {if (a[i]<lo) lo=a[i]; if (a[i]>hi) hi=a[i];}
  if (lo==hi) { *out=lo; return 1; }
  mode=lo;
  for (j=0;j<grid;++j) {
    double x=(double)((long double)lo+((long double)hi-lo)*j/(grid-1)); long double score=0;
    for (i=0;i<n;++i) { long double z=((long double)x-a[i])/h; if (fabsl(z)<40) score+=expl(-0.5L*z*z); }
    if ((double)score>best) {best=(double)score; mode=x;}
  }
  if (!isfinite(mode)) return 0;
  *out=mode; return 1;
}
int mulib_statsproc_theil_sen(const double *x, const double *y, size_t n, double *slope, double *intercept) {
  size_t i,j,k=0,cap; double *slopes, *offsets, b, a;
  if (!slope || !intercept || slope==intercept || n<2 || n>1024 || !mulib_statsproc_valid_(x,n,0) || !mulib_statsproc_valid_(y,n,0)) return 0;
  cap=n*(n-1)/2;
  slopes=(double *)malloc(cap*sizeof(double)); offsets=(double *)malloc(n*sizeof(double));
  if (!slopes || !offsets) {free(slopes);free(offsets);return 0;}
  for (i=0;i<n;++i) for (j=i+1;j<n;++j) if (x[i]!=x[j]) {
    double t=(double)(((long double)y[j]-y[i])/((long double)x[j]-x[i]));
    if (!isfinite(t)) {free(slopes);free(offsets);return 0;}
    slopes[k++]=t;
  }
  if (!k) { free(slopes);free(offsets);return 0; }
  qsort(slopes,k,sizeof(*slopes),mulib_statsproc_cmp_);
  b=mulib_statsproc_qsorted_(slopes,k,0.5);
  for (i=0;i<n;++i) {
    offsets[i]=(double)((long double)y[i]-(long double)b*x[i]);
    if (!isfinite(offsets[i])) {free(slopes);free(offsets);return 0;}
  }
  qsort(offsets,n,sizeof(*offsets),mulib_statsproc_cmp_); a=mulib_statsproc_qsorted_(offsets,n,0.5);
  free(slopes);free(offsets); if (!isfinite(a)||!isfinite(b)) return 0;
  *slope=b;*intercept=a;return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
