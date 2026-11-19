/* MuLIB / Amb3r: spice-nonlinear. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_NONLINEAR in exactly one implementation translation unit.
 * Profile: Bounded damped device-stamping DC iteration using dense solve; no full SPICE convergence ladder.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_NONLINEAR_H
#define MULIB_SPICE_NONLINEAR_H
#include "mulib-amb3r-base.h"
#include "mulib-spice-device.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_spice_nonlinear_op(const ma_spice_device *devices,size_t nd,size_t n,double *x,double time,double tol,size_t limit);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_NONLINEAR) && !defined(MULIB_SPICE_NONLINEAR_H_IMPLEMENTED)
#define MULIB_SPICE_NONLINEAR_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_nonlinear_op(const ma_spice_device *devices,size_t nd,size_t n,double *x,double time,double tol,size_t limit){if(!n||!x||tol<=0||!isfinite(tol))return 0;ma_mna m={0};if(!mulib_spice_mna_init(&m,n))return 0;double *next=(double*)ma_alloc(n,sizeof(double)),*guess=(double*)ma_alloc(n,sizeof(double));if(!next||!guess){free(next);free(guess);mulib_spice_mna_free(&m);return 0;}memcpy(guess,x,n*sizeof(double));int ok=0;for(size_t k=0;k<limit;k++){memset(m.g,0,n*n*sizeof(double));memset(m.c,0,n*n*sizeof(double));memset(m.b,0,n*sizeof(double));if(!mulib_spice_device_stamp(devices,nd,&m,guess,time)||!mulib_spice_mna_solve(&m,next,DBL_MIN))break;double err=0;for(size_t i=0;i<n;i++)err=MA_MAX(err,fabs(next[i]-guess[i])/(1+fabs(next[i])));if(err<=tol){memcpy(x,next,n*sizeof(double));ok=1;break;}double relax=err>1?.5:1;for(size_t i=0;i<n;i++)guess[i]+=relax*(next[i]-guess[i]);}free(next);free(guess);mulib_spice_mna_free(&m);return ok;}
#ifdef __cplusplus
}
#endif
#endif
