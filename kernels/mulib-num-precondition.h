/* MuLIB: num-precondition. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_PRECONDITION in exactly one implementation translation unit.
 * Profile: Jacobi diagonal preconditioner; no incomplete LU/Cholesky implementation.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_PRECONDITION_H
#define MULIB_NUM_PRECONDITION_H
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
#include "mulib-num-core.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_num_precondition_jacobi(const mulib_num_csr_view *a,double tolerance,double *inverse);
int mulib_num_precondition_apply(size_t n,const double *inverse,const double *b,double *x);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_PRECONDITION) && !defined(MULIB_NUM_PRECONDITION_H_IMPLEMENTED)
#define MULIB_NUM_PRECONDITION_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_precondition_jacobi(const mulib_num_csr_view *a,double tolerance,double *inverse){if(!mulib_num_csr_view_valid(a)||a->rows!=a->cols||!inverse||tolerance<0)return 0;for(size_t i=0;i<a->rows;i++){double d=0;for(size_t k=a->ptr[i];k<a->ptr[i+1];k++)if(a->idx[k]==i)d+=a->val[k];if(fabs(d)<=tolerance||!isfinite(d))return 0;inverse[i]=1/d;}return 1;}
int mulib_num_precondition_apply(size_t n,const double *inverse,const double *b,double *x){if(!inverse||!b||!x)return 0;for(size_t i=0;i<n;i++)x[i]=inverse[i]*b[i];return mulib_num_all_finite(x,n);}
#ifdef __cplusplus
}
#endif
#endif
