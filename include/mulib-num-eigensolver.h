/* MuLIB / Amb3r: num-eigensolver. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_EIGENSOLVER in exactly one implementation translation unit.
 * Profile: Power iteration for dominant eigenpair, not a general eigensolver.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_EIGENSOLVER_H
#define MULIB_NUM_EIGENSOLVER_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_num_eigensolver_power(const ma_csr *a,double *vector,double *eigenvalue,size_t limit,double tol);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_EIGENSOLVER) && !defined(MULIB_NUM_EIGENSOLVER_H_IMPLEMENTED)
#define MULIB_NUM_EIGENSOLVER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_eigensolver_power(const ma_csr *a,double *vector,double *eigenvalue,size_t limit,double tol){if(!ma_csr_valid(a)||a->rows!=a->cols||!vector||!eigenvalue||tol<=0||!a->rows)return 0;size_t n=a->rows;double *v=(double*)ma_alloc(n,sizeof(double)),*w=(double*)ma_alloc(n,sizeof(double));if(!v||!w){free(v);free(w);return 0;}memcpy(v,vector,n*sizeof(double));double d=ma_norm(v,n);if(!d){for(size_t i=0;i<n;i++)v[i]=1/sqrt((double)n);}else for(size_t i=0;i<n;i++)v[i]/=d;int ok=0;for(size_t k=0;k<limit;k++){ma_csr_mv(a,v,w);double lam=ma_dot(v,w,n),norm=ma_norm(w,n),err=0;for(size_t i=0;i<n;i++)err=hypot(err,w[i]-lam*v[i]);if(err<=tol*MA_MAX(1,fabs(lam))){*eigenvalue=lam;memcpy(vector,v,n*sizeof(double));ok=1;break;}if(!norm||!isfinite(norm))break;for(size_t i=0;i<n;i++)v[i]=w[i]/norm;}free(v);free(w);return ok;}
#ifdef __cplusplus
}
#endif
#endif
