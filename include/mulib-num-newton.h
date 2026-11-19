/* MuLIB / Amb3r: num-newton. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_NEWTON in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_NEWTON_H
#define MULIB_NUM_NEWTON_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef int (*ma_residual)(void *,const double *,size_t,double *,double *);
int mulib_num_newton(ma_residual f,void *ctx,double *x,size_t n,double tolerance,size_t limit,size_t *iters);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_NEWTON) && !defined(MULIB_NUM_NEWTON_H_IMPLEMENTED)
#define MULIB_NUM_NEWTON_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_newton(ma_residual f,void *ctx,double *x,size_t n,double tolerance,size_t limit,size_t *iters){size_t nn;if(!f||!x||!n||!ma_mul(n,n,&nn)||tolerance<=0||!isfinite(tolerance))return 0;double *j=(double*)ma_alloc(nn,sizeof(double)),*r=(double*)ma_alloc(n,sizeof(double)),*d=(double*)ma_alloc(n,sizeof(double)),*t=(double*)ma_alloc(n,sizeof(double)),*rr=(double*)ma_alloc(n,sizeof(double));if(!j||!r||!d||!t||!rr){free(j);free(r);free(d);free(t);free(rr);return 0;}size_t k=0;int ok=0;for(;k<=limit;k++){if(!f(ctx,x,n,r,j)||!ma_finite(r,n))break;double norm=ma_norm(r,n);if(norm<=tolerance){ok=1;break;}if(k==limit)break;for(size_t i=0;i<n;i++)rr[i]=-r[i];if(!ma_solve(n,j,rr,d,DBL_MIN))break;double a=1;int accepted=0;for(size_t z=0;z<40;z++,a*=.5){for(size_t i=0;i<n;i++)t[i]=x[i]+a*d[i];if(f(ctx,t,n,rr,j)&&ma_finite(rr,n)&&ma_norm(rr,n)<norm){memcpy(x,t,n*sizeof(double));accepted=1;break;}}if(!accepted)break;}if(iters)*iters=k;free(j);free(r);free(d);free(t);free(rr);return ok;}
#ifdef __cplusplus
}
#endif
#endif
