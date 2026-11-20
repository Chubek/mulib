/* MuLIB: num-dae. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_DAE in exactly one implementation translation unit.
 * Profile: Backward Euler for constant linear mass/stiffness DAE; not a general nonlinear DAE engine.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_DAE_H
#define MULIB_NUM_DAE_H
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

int mulib_num_dae_step(size_t n,const double *mass,const double *g,const double *source,const double *previous,double dt,double *next);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_DAE) && !defined(MULIB_NUM_DAE_H_IMPLEMENTED)
#define MULIB_NUM_DAE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_dae_step(size_t n,const double *mass,const double *g,const double *source,const double *previous,double dt,double *next){size_t nn;if(!n||!mulib_storage_size_mul(n,n,&nn)||!mass||!g||!source||!previous||!next||dt<=0||!isfinite(dt))return 0;double *a=(double*)mulib_storage_alloc(nn,sizeof(double)),*b=(double*)mulib_storage_alloc(n,sizeof(double));if(!a||!b){free(a);free(b);return 0;}for(size_t i=0;i<n;i++){b[i]=source[i];for(size_t j=0;j<n;j++){a[i*n+j]=g[i*n+j]+mass[i*n+j]/dt;b[i]+=mass[i*n+j]*previous[j]/dt;}}int ok=mulib_num_dense_reference_solve(n,a,b,next,DBL_MIN);free(a);free(b);return ok;}
#ifdef __cplusplus
}
#endif
#endif
