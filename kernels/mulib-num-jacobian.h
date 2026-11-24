/* MuLIB: num-jacobian. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_JACOBIAN in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_JACOBIAN_H
#define MULIB_NUM_JACOBIAN_H
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
#ifdef __cplusplus
extern "C" {
#endif
typedef int (*mulib_num_vector_function)(void *,const double *,size_t,double *,size_t);
int mulib_num_jacobian(mulib_num_vector_function f,void *ctx,const double *x,size_t n,size_t m,double relative_step,double *jac);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_JACOBIAN) && !defined(MULIB_NUM_JACOBIAN_H_IMPLEMENTED)
#define MULIB_NUM_JACOBIAN_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_jacobian(mulib_num_vector_function f,void *ctx,const double *x,size_t n,size_t m,double relative_step,double *jac){if(!f||!x||!n||!m||relative_step<=0||!isfinite(relative_step)||!jac)return 0;double *t=(double*)mulib_storage_alloc(n,sizeof(double)),*p=(double*)mulib_storage_alloc(m,sizeof(double)),*q=(double*)mulib_storage_alloc(m,sizeof(double));if(!t||!p||!q){free(t);free(p);free(q);return 0;}memcpy(t,x,n*sizeof(double));int ok=1;for(size_t k=0;k<n&&ok;k++){double h=relative_step*MULIB_STORAGE_MAX(1,fabs(x[k]));t[k]=x[k]+h;double hi=t[k];ok=f(ctx,t,n,p,m);t[k]=x[k]-h;double lo=t[k];ok=ok&&f(ctx,t,n,q,m)&&hi!=lo&&mulib_num_all_finite(p,m)&&mulib_num_all_finite(q,m);t[k]=x[k];if(ok)for(size_t i=0;i<m;i++)jac[i*n+k]=(p[i]-q[i])/(hi-lo);}free(t);free(p);free(q);return ok;}
#ifdef __cplusplus
}
#endif
#endif
