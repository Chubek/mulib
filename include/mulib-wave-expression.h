/* MuLIB / Amb3r: wave-expression. C11 single-header kernel.
 * Define MULIB_IMPL_WAVE_EXPRESSION in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_WAVE_EXPRESSION_H
#define MULIB_WAVE_EXPRESSION_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_wave_expression_binary(const double *a,const double *b,size_t n,unsigned op,double *out);
int mulib_wave_expression_integrate(const double *time,const double *value,size_t n,double *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WAVE_EXPRESSION) && !defined(MULIB_WAVE_EXPRESSION_H_IMPLEMENTED)
#define MULIB_WAVE_EXPRESSION_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_wave_expression_binary(const double *a,const double *b,size_t n,unsigned op,double *out){if(!ma_finite(a,n)||!ma_finite(b,n)||(!out&&n)||op>3)return 0;for(size_t i=0;i<n;i++){double x=op==0?a[i]+b[i]:op==1?a[i]-b[i]:op==2?a[i]*b[i]:b[i]?a[i]/b[i]:NAN;if(!isfinite(x))return 0;out[i]=x;}return 1;}
int mulib_wave_expression_integrate(const double *time,const double *value,size_t n,double *out){if(!n||!ma_finite(time,n)||!ma_finite(value,n)||!out)return 0;out[0]=0;for(size_t i=1;i<n;i++){if(time[i]<=time[i-1])return 0;out[i]=out[i-1]+.5*(value[i]+value[i-1])*(time[i]-time[i-1]);}return ma_finite(out,n);}
#ifdef __cplusplus
}
#endif
#endif
