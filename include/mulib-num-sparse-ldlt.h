/* MuLIB / Amb3r: num-sparse-ldlt. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_SPARSE_LDLT in exactly one implementation translation unit.
 * Profile: Unpivoted symmetric factorization; rejects tiny/zero pivots, no 2x2 pivoting.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_SPARSE_LDLT_H
#define MULIB_NUM_SPARSE_LDLT_H
#include "mulib-amb3r-base.h"
#include "mulib-num-sparse-lu.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_num_sparse_ldlt(const ma_csr *a,double tol,ma_lu_row *l,double *d);
int mulib_num_sparse_ldlt_solve(size_t n,const ma_lu_row *l,const double *d,const double *b,double *x);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_SPARSE_LDLT) && !defined(MULIB_NUM_SPARSE_LDLT_H_IMPLEMENTED)
#define MULIB_NUM_SPARSE_LDLT_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_sparse_ldlt(const ma_csr *a,double tol,ma_lu_row *l,double *d){if(!ma_csr_valid(a)||a->rows!=a->cols||!l||!d||tol<0)return 0;size_t n=a->rows;for(size_t i=0;i<n;i++)if(l[i].v)return 0;for(size_t i=0;i<n;i++){for(size_t j=0;j<=i;j++){double s=0;for(size_t k=a->ptr[i];k<a->ptr[i+1];k++)if(a->idx[k]==j)s+=a->val[k];for(size_t k=0;k<j;k++)s-=ma_lu_get(l+i,k)*d[k]*ma_lu_get(l+j,k);if(i==j){if(!isfinite(s)||fabs(s)<=tol)goto fail;d[i]=s;}else if(!ma_lu_set(l+i,j,s/d[j]))goto fail;}}return 1;fail:for(size_t i=0;i<n;i++){free(l[i].v);memset(l+i,0,sizeof *l);}return 0;}
int mulib_num_sparse_ldlt_solve(size_t n,const ma_lu_row *l,const double *d,const double *b,double *x){if(!l||!d||!b||!x)return 0;double *r=(double*)ma_alloc(n,sizeof(double));if(!r)return 0;for(size_t i=0;i<n;i++){r[i]=b[i];for(size_t j=0;j<i;j++)r[i]-=ma_lu_get(l+i,j)*r[j];}for(size_t i=0;i<n;i++){if(!d[i]){free(r);return 0;}r[i]/=d[i];}for(size_t i=n;i-->0;)for(size_t j=i+1;j<n;j++)r[i]-=ma_lu_get(l+j,i)*r[j];int ok=ma_finite(r,n);if(ok)memcpy(x,r,n*sizeof(double));free(r);return ok;}
#ifdef __cplusplus
}
#endif
#endif
