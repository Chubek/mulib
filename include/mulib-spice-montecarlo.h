/* MuLIB / Amb3r: spice-montecarlo. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_MONTECARLO in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_MONTECARLO_H
#define MULIB_SPICE_MONTECARLO_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_spice_montecarlo(ma_rng *rng,const double *mean,const double *sigma,size_t n,double *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_MONTECARLO) && !defined(MULIB_SPICE_MONTECARLO_H_IMPLEMENTED)
#define MULIB_SPICE_MONTECARLO_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_montecarlo(ma_rng *rng,const double *mean,const double *sigma,size_t n,double *out){if(!rng||(!mean&&n)||(!sigma&&n)||(!out&&n))return 0;for(size_t i=0;i<n;i++){if(sigma[i]<0||!isfinite(mean[i]+sigma[i]))return 0;double u=1-ma_uniform(rng),v=ma_uniform(rng);out[i]=mean[i]+sigma[i]*sqrt(-2*log(u))*cos(2*MA_PI*v);}return ma_finite(out,n);}
#ifdef __cplusplus
}
#endif
#endif
