/* MuLIB: num-cholesky. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_CHOLESKY in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_CHOLESKY_H
#define MULIB_NUM_CHOLESKY_H
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

int mulib_num_cholesky_factor(size_t n,const double *a,double *l,double tol);
int mulib_num_cholesky_solve(size_t n,const double *l,const double *b,double *x);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_CHOLESKY) && !defined(MULIB_NUM_CHOLESKY_H_IMPLEMENTED)
#define MULIB_NUM_CHOLESKY_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_cholesky_factor(size_t n,const double *a,double *l,double tol){size_t nn;if(!n||!mulib_storage_size_mul(n,n,&nn)||!mulib_num_all_finite(a,nn)||!l||tol<0)return 0;double *r=(double*)mulib_storage_alloc(nn,sizeof(double));if(!r)return 0;for(size_t i=0;i<n;i++)for(size_t j=0;j<=i;j++){if(fabs(a[i*n+j]-a[j*n+i])>tol){free(r);return 0;}double x=a[i*n+j];for(size_t k=0;k<j;k++)x-=r[i*n+k]*r[j*n+k];if(i==j){if(x<=tol||!isfinite(x)){free(r);return 0;}r[i*n+j]=sqrt(x);}else r[i*n+j]=x/r[j*n+j];}memcpy(l,r,nn*sizeof(double));free(r);return 1;}
int mulib_num_cholesky_solve(size_t n,const double *l,const double *b,double *x){if(!n||!l||!b||!x)return 0;double *r=(double*)mulib_storage_alloc(n,sizeof *r);if(!r)return 0;for(size_t i=0;i<n;i++){if(l[i*n+i]<=0){free(r);return 0;}double s=b[i];for(size_t j=0;j<i;j++)s-=l[i*n+j]*r[j];r[i]=s/l[i*n+i];}for(size_t i=n;i-->0;){double s=r[i];for(size_t j=i+1;j<n;j++)s-=l[j*n+i]*r[j];r[i]=s/l[i*n+i];}int ok=mulib_num_all_finite(r,n);if(ok)memcpy(x,r,n*sizeof(double));free(r);return ok;}
#ifdef __cplusplus
}
#endif
#endif
