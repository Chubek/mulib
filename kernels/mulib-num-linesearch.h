/* MuLIB: num-linesearch. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_LINESEARCH in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_LINESEARCH_H
#define MULIB_NUM_LINESEARCH_H
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
#ifdef __cplusplus
extern "C" {
#endif
typedef double (*mulib_num_objective)(void *,const double *,size_t);
int mulib_num_linesearch_armijo(mulib_num_objective fn,void *ctx,const double *x,const double *direction,size_t n,double value,double slope,double c,double shrink,size_t max_iter,double *alpha,double *trial);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_LINESEARCH) && !defined(MULIB_NUM_LINESEARCH_H_IMPLEMENTED)
#define MULIB_NUM_LINESEARCH_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_linesearch_armijo(mulib_num_objective fn,void *ctx,const double *x,const double *direction,size_t n,double value,double slope,double c,double shrink,size_t max_iter,double *alpha,double *trial){if(!fn||!x||!direction||!alpha||!trial||!n||!isfinite(value)||slope>=0||c<=0||c>=1||shrink<=0||shrink>=1)return 0;double a=1;for(size_t k=0;k<max_iter;k++){for(size_t i=0;i<n;i++)trial[i]=x[i]+a*direction[i];double v=fn(ctx,trial,n);if(isfinite(v)&&v<=value+c*a*slope){*alpha=a;return 1;}a*=shrink;}return 0;}
#ifdef __cplusplus
}
#endif
#endif
