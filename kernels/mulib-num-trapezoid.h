/* MuLIB: num-trapezoid. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_TRAPEZOID in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_TRAPEZOID_H
#define MULIB_NUM_TRAPEZOID_H
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

int mulib_num_trapezoid_step(size_t n,const double *mass,const double *g,const double *src0,const double *src1,const double *prev,double dt,double *next);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_TRAPEZOID) && !defined(MULIB_NUM_TRAPEZOID_H_IMPLEMENTED)
#define MULIB_NUM_TRAPEZOID_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_trapezoid_step(size_t n,const double *mass,const double *g,const double *src0,const double *src1,const double *prev,double dt,double *next){size_t nn;if(!n||!mulib_storage_size_mul(n,n,&nn)||!mass||!g||!src0||!src1||!prev||!next||dt<=0||!isfinite(dt))return 0;double *a=(double*)mulib_storage_alloc(nn,sizeof(double)),*b=(double*)mulib_storage_alloc(n,sizeof(double));if(!a||!b){free(a);free(b);return 0;}for(size_t i=0;i<n;i++){b[i]=src0[i]+src1[i];for(size_t j=0;j<n;j++){a[i*n+j]=g[i*n+j]+2*mass[i*n+j]/dt;b[i]+=(2*mass[i*n+j]/dt-g[i*n+j])*prev[j];}}int ok=mulib_num_dense_reference_solve(n,a,b,next,DBL_MIN);free(a);free(b);return ok;}
#ifdef __cplusplus
}
#endif
#endif
