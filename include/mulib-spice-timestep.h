/* MuLIB: spice-timestep. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_TIMESTEP in exactly one implementation translation unit.
 * Profile: Adaptive step-doubling for linear backward Euler DAE only.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_TIMESTEP_H
#define MULIB_SPICE_TIMESTEP_H
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
#include "mulib-storage-core.h"
#include "mulib-num-core.h"
#include "mulib-geom2-core.h"
#include "mulib-graph-core.h"
#include "mulib-spice-dae.h"
#include "mulib-num-adaptive.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_spice_timestep_linear(const mulib_spice_mna *m,const double *previous,double *next,double *dt,double minimum,double maximum,double atol,double rtol,size_t attempts);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_TIMESTEP) && !defined(MULIB_SPICE_TIMESTEP_H_IMPLEMENTED)
#define MULIB_SPICE_TIMESTEP_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_timestep_linear(const mulib_spice_mna *m,const double *previous,double *next,double *dt,double minimum,double maximum,double atol,double rtol,size_t attempts){if(!m||!previous||!next||!dt||!isfinite(*dt)||*dt<minimum||minimum<=0||maximum<minimum||atol<=0||rtol<0)return 0;size_t n=m->n;double *a=(double*)mulib_storage_alloc(n,sizeof(double)),*b=(double*)mulib_storage_alloc(n,sizeof(double)),*c=(double*)mulib_storage_alloc(n,sizeof(double));if(!a||!b||!c){free(a);free(b);free(c);return 0;}double h=MULIB_STORAGE_MIN(*dt,maximum);int ok=0;for(size_t k=0;k<attempts;k++){if(!mulib_spice_dae_step(m,previous,h,a)||!mulib_spice_dae_step(m,previous,h/2,b)||!mulib_spice_dae_step(m,b,h/2,c))break;double e=mulib_num_adaptive_error(a,c,n,atol,rtol);if(e<=1){memcpy(next,c,n*sizeof(double));*dt=h;ok=1;break;}if(h<=minimum||!isfinite(e))break;h=MULIB_STORAGE_MAX(minimum,h*MULIB_STORAGE_MAX(.1,.8/sqrt(e)));}free(a);free(b);free(c);return ok;}
#ifdef __cplusplus
}
#endif
#endif
