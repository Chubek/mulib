/* MuLIB: spice-sensitivity. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_SENSITIVITY in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_SENSITIVITY_H
#define MULIB_SPICE_SENSITIVITY_H
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

int mulib_spice_sensitivity(size_t n,const double *matrix,const double *derivative_matrix,const double *derivative_rhs,const double *solution,double *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_SENSITIVITY) && !defined(MULIB_SPICE_SENSITIVITY_H_IMPLEMENTED)
#define MULIB_SPICE_SENSITIVITY_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_sensitivity(size_t n,const double *matrix,const double *derivative_matrix,const double *derivative_rhs,const double *solution,double *out){size_t nn;if(!n||!mulib_storage_size_mul(n,n,&nn)||!matrix||!derivative_matrix||!derivative_rhs||!solution||!out)return 0;double *b=(double*)mulib_storage_alloc(n,sizeof(double));if(!b)return 0;for(size_t i=0;i<n;i++){b[i]=derivative_rhs[i];for(size_t j=0;j<n;j++)b[i]-=derivative_matrix[i*n+j]*solution[j];}int ok=mulib_num_dense_reference_solve(n,matrix,b,out,DBL_MIN);free(b);return ok;}
#ifdef __cplusplus
}
#endif
#endif
