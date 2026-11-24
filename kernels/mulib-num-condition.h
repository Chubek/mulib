/* MuLIB: num-condition. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_CONDITION in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_CONDITION_H
#define MULIB_NUM_CONDITION_H
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

double mulib_num_condition_residual(const mulib_num_csr_view *a,const double *x,const double *b);
double mulib_num_condition_inf(size_t n,const double *a);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_CONDITION) && !defined(MULIB_NUM_CONDITION_H_IMPLEMENTED)
#define MULIB_NUM_CONDITION_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
double mulib_num_condition_residual(const mulib_num_csr_view *a,const double *x,const double *b){if(!mulib_num_csr_view_valid(a)||!x||!b)return NAN;double norm=0;for(size_t i=0;i<a->rows;i++){double s=-b[i];for(size_t k=a->ptr[i];k<a->ptr[i+1];k++)s+=a->val[k]*x[a->idx[k]];norm=hypot(norm,s);}return norm;}
double mulib_num_condition_inf(size_t n,const double *a){size_t nn;if(!n||!mulib_storage_size_mul(n,n,&nn)||!mulib_num_all_finite(a,nn))return NAN;double *b=(double*)mulib_storage_alloc(n,sizeof(double)),*x=(double*)mulib_storage_alloc(n,sizeof(double)),*rows=(double*)mulib_storage_alloc(n,sizeof(double));if(!b||!x||!rows){free(b);free(x);free(rows);return NAN;}double an=0,inv=0;for(size_t i=0;i<n;i++){double s=0;for(size_t j=0;j<n;j++)s+=fabs(a[i*n+j]);an=MULIB_STORAGE_MAX(an,s);b[i]=1;if(!mulib_num_dense_reference_solve(n,a,b,x,0)){inv=INFINITY;goto end;}b[i]=0;for(size_t j=0;j<n;j++)rows[j]+=fabs(x[j]);}for(size_t i=0;i<n;i++)inv=MULIB_STORAGE_MAX(inv,rows[i]);end:free(b);free(x);free(rows);return an==0?INFINITY:an*inv;}
#ifdef __cplusplus
}
#endif
#endif
