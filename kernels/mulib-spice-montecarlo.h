/* MuLIB: spice-montecarlo. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_MONTECARLO in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_MONTECARLO_H
#define MULIB_SPICE_MONTECARLO_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-num-core.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_spice_montecarlo(mulib_num_rng *rng,const double *mean,const double *sigma,size_t n,double *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_MONTECARLO) && !defined(MULIB_SPICE_MONTECARLO_H_IMPLEMENTED)
#define MULIB_SPICE_MONTECARLO_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_montecarlo(mulib_num_rng *rng,const double *mean,const double *sigma,size_t n,double *out){if(!rng||(!mean&&n)||(!sigma&&n)||(!out&&n))return 0;for(size_t i=0;i<n;i++){if(sigma[i]<0||!isfinite(mean[i]+sigma[i]))return 0;double u=1-mulib_num_rng_uniform(rng),v=mulib_num_rng_uniform(rng);out[i]=mean[i]+sigma[i]*sqrt(-2*log(u))*cos(2*MULIB_NUM_PI*v);}return mulib_num_all_finite(out,n);}
#ifdef __cplusplus
}
#endif
#endif
