/* MuLIB / Amb3r: field-fdtd. C11 single-header kernel.
 * Define MULIB_IMPL_FIELD_FDTD in exactly one implementation translation unit.
 * Profile: 1-D Yee scheme, uniform material, PEC boundaries; not full-wave 3-D solver.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FIELD_FDTD_H
#define MULIB_FIELD_FDTD_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_field_fdtd_step(double *e,double *h,size_t n,double dx,double dt,double epsilon,double mu);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FIELD_FDTD) && !defined(MULIB_FIELD_FDTD_H_IMPLEMENTED)
#define MULIB_FIELD_FDTD_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_field_fdtd_step(double *e,double *h,size_t n,double dx,double dt,double epsilon,double mu){if(n<3||!ma_finite(e,n)||!ma_finite(h,n-1)||dx<=0||dt<=0||epsilon<=0||mu<=0||!isfinite(dx+dt+epsilon+mu)||dt>dx*sqrt(epsilon*mu))return 0;for(size_t i=0;i+1<n;i++)h[i]-=dt/(mu*dx)*(e[i+1]-e[i]);for(size_t i=1;i+1<n;i++)e[i]-=dt/(epsilon*dx)*(h[i]-h[i-1]);e[0]=e[n-1]=0;return ma_finite(e,n)&&ma_finite(h,n-1);}
#ifdef __cplusplus
}
#endif
#endif
