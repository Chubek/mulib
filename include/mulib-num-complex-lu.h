/* MuLIB / Amb3r: num-complex-lu. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_COMPLEX_LU in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_COMPLEX_LU_H
#define MULIB_NUM_COMPLEX_LU_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_num_complex_lu_solve(size_t n,const ma_complex *a,const ma_complex *b,ma_complex *x,double tol);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_COMPLEX_LU) && !defined(MULIB_NUM_COMPLEX_LU_H_IMPLEMENTED)
#define MULIB_NUM_COMPLEX_LU_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_complex_lu_solve(size_t n,const ma_complex *a,const ma_complex *b,ma_complex *x,double tol){size_t nn;if(!n||!ma_mul(n,n,&nn)||!a||!b||!x||tol<0)return 0;ma_complex *m=(ma_complex*)ma_alloc(nn,sizeof *m),*r=(ma_complex*)ma_alloc(n,sizeof *r);if(!m||!r){free(m);free(r);return 0;}memcpy(m,a,nn*sizeof *m);memcpy(r,b,n*sizeof *r);int ok=1;for(size_t k=0;k<n;k++){size_t p=k;for(size_t i=k+1;i<n;i++)if(ma_cabs(m[i*n+k])>ma_cabs(m[p*n+k]))p=i;if(ma_cabs(m[p*n+k])<=tol||!isfinite(ma_cabs(m[p*n+k]))){ok=0;break;}if(p!=k){for(size_t j=k;j<n;j++){ma_complex t=m[p*n+j];m[p*n+j]=m[k*n+j];m[k*n+j]=t;}ma_complex t=r[p];r[p]=r[k];r[k]=t;}for(size_t i=k+1;i<n;i++){ma_complex f=ma_cdiv(m[i*n+k],m[k*n+k]);for(size_t j=k+1;j<n;j++)m[i*n+j]=ma_csub(m[i*n+j],ma_cmul(f,m[k*n+j]));r[i]=ma_csub(r[i],ma_cmul(f,r[k]));}}if(ok){for(size_t i=n;i-->0;){for(size_t j=i+1;j<n;j++)r[i]=ma_csub(r[i],ma_cmul(m[i*n+j],r[j]));r[i]=ma_cdiv(r[i],m[i*n+i]);if(!isfinite(r[i].re)||!isfinite(r[i].im))ok=0;}if(ok)memcpy(x,r,n*sizeof *r);}free(m);free(r);return ok;}
#ifdef __cplusplus
}
#endif
#endif
