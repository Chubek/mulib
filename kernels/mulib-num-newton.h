/* MuLIB: num-newton. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_NEWTON in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_NEWTON_H
#define MULIB_NUM_NEWTON_H
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
#include "mulib-graph-core.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef int (*mulib_num_residual)(void *,const double *,size_t,double *,double *);
int mulib_num_newton(mulib_num_residual f,void *ctx,double *x,size_t n,double tolerance,size_t limit,size_t *iters);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_NEWTON) && !defined(MULIB_NUM_NEWTON_H_IMPLEMENTED)
#define MULIB_NUM_NEWTON_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_newton(mulib_num_residual f,void *ctx,double *x,size_t n,double tolerance,size_t limit,size_t *iters){size_t nn;if(!f||!x||!n||!mulib_storage_size_mul(n,n,&nn)||tolerance<=0||!isfinite(tolerance))return 0;double *j=(double*)mulib_storage_alloc(nn,sizeof(double)),*r=(double*)mulib_storage_alloc(n,sizeof(double)),*d=(double*)mulib_storage_alloc(n,sizeof(double)),*t=(double*)mulib_storage_alloc(n,sizeof(double)),*rr=(double*)mulib_storage_alloc(n,sizeof(double));if(!j||!r||!d||!t||!rr){free(j);free(r);free(d);free(t);free(rr);return 0;}size_t k=0;int ok=0;for(;k<=limit;k++){if(!f(ctx,x,n,r,j)||!mulib_num_all_finite(r,n))break;double norm=mulib_num_norm(r,n);if(norm<=tolerance){ok=1;break;}if(k==limit)break;for(size_t i=0;i<n;i++)rr[i]=-r[i];if(!mulib_num_dense_reference_solve(n,j,rr,d,DBL_MIN))break;double a=1;int accepted=0;for(size_t z=0;z<40;z++,a*=.5){for(size_t i=0;i<n;i++)t[i]=x[i]+a*d[i];if(f(ctx,t,n,rr,j)&&mulib_num_all_finite(rr,n)&&mulib_num_norm(rr,n)<norm){memcpy(x,t,n*sizeof(double));accepted=1;break;}}if(!accepted)break;}if(iters)*iters=k;free(j);free(r);free(d);free(t);free(rr);return ok;}
#ifdef __cplusplus
}
#endif
#endif
