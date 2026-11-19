/* MuLIB / Amb3r: num-condition. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_CONDITION in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_CONDITION_H
#define MULIB_NUM_CONDITION_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

double mulib_num_condition_residual(const ma_csr *a,const double *x,const double *b);
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
double mulib_num_condition_residual(const ma_csr *a,const double *x,const double *b){if(!ma_csr_valid(a)||!x||!b)return NAN;double norm=0;for(size_t i=0;i<a->rows;i++){double s=-b[i];for(size_t k=a->ptr[i];k<a->ptr[i+1];k++)s+=a->val[k]*x[a->idx[k]];norm=hypot(norm,s);}return norm;}
double mulib_num_condition_inf(size_t n,const double *a){size_t nn;if(!n||!ma_mul(n,n,&nn)||!ma_finite(a,nn))return NAN;double *b=(double*)ma_alloc(n,sizeof(double)),*x=(double*)ma_alloc(n,sizeof(double)),*rows=(double*)ma_alloc(n,sizeof(double));if(!b||!x||!rows){free(b);free(x);free(rows);return NAN;}double an=0,inv=0;for(size_t i=0;i<n;i++){double s=0;for(size_t j=0;j<n;j++)s+=fabs(a[i*n+j]);an=MA_MAX(an,s);b[i]=1;if(!ma_solve(n,a,b,x,0)){inv=INFINITY;goto end;}b[i]=0;for(size_t j=0;j<n;j++)rows[j]+=fabs(x[j]);}for(size_t i=0;i<n;i++)inv=MA_MAX(inv,rows[i]);end:free(b);free(x);free(rows);return an==0?INFINITY:an*inv;}
#ifdef __cplusplus
}
#endif
#endif
