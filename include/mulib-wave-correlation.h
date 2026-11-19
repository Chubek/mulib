/* MuLIB / Amb3r: wave-correlation. C11 single-header kernel.
 * Define MULIB_IMPL_WAVE_CORRELATION in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_WAVE_CORRELATION_H
#define MULIB_WAVE_CORRELATION_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_wave_correlation(const double *a,const double *b,size_t n,ptrdiff_t lag,double *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WAVE_CORRELATION) && !defined(MULIB_WAVE_CORRELATION_H_IMPLEMENTED)
#define MULIB_WAVE_CORRELATION_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_wave_correlation(const double *a,const double *b,size_t n,ptrdiff_t lag,double *out){if(!a||!b||!out||!n||!ma_finite(a,n)||!ma_finite(b,n))return 0;uintmax_t l=lag<0?(uintmax_t)(-(lag+1))+1:(uintmax_t)lag;if(l>=n)return 0;size_t off=(size_t)l,k=n-off;const double *x=a+(lag<0?off:0),*y=b+(lag>0?off:0);double mx=0,my=0;for(size_t i=0;i<k;i++){mx+=(x[i]-mx)/(i+1);my+=(y[i]-my)/(i+1);}double s=0,xx=0,yy=0;for(size_t i=0;i<k;i++){double u=x[i]-mx,v=y[i]-my;s+=u*v;xx+=u*u;yy+=v*v;}if(xx<=0||yy<=0)return 0;*out=s/sqrt(xx*yy);return isfinite(*out);}
#ifdef __cplusplus
}
#endif
#endif
