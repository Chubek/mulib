/* MuLIB / Amb3r: spice-sweep. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_SWEEP in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_SWEEP_H
#define MULIB_SPICE_SWEEP_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_spice_sweep(double start,double stop,size_t n,int logarithmic,double *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_SWEEP) && !defined(MULIB_SPICE_SWEEP_H_IMPLEMENTED)
#define MULIB_SPICE_SWEEP_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_sweep(double start,double stop,size_t n,int logarithmic,double *out){if(!n||!out||!isfinite(start)||!isfinite(stop)||(logarithmic&&(start<=0||stop<=0)))return 0;for(size_t i=0;i<n;i++){double t=n==1?0:(double)i/(n-1);out[i]=logarithmic?exp((1-t)*log(start)+t*log(stop)):(1-t)*start+t*stop;}return ma_finite(out,n);}
#ifdef __cplusplus
}
#endif
#endif
