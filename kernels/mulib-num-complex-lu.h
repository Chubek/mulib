/* MuLIB: num-complex-lu. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_COMPLEX_LU in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_COMPLEX_LU_H
#define MULIB_NUM_COMPLEX_LU_H
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

int mulib_num_complex_lu_solve(size_t n,const mulib_num_complex *a,const mulib_num_complex *b,mulib_num_complex *x,double tol);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_COMPLEX_LU) && !defined(MULIB_NUM_COMPLEX_LU_H_IMPLEMENTED)
#define MULIB_NUM_COMPLEX_LU_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_complex_lu_solve(size_t n,const mulib_num_complex *a,const mulib_num_complex *b,mulib_num_complex *x,double tol){size_t nn;if(!n||!mulib_storage_size_mul(n,n,&nn)||!a||!b||!x||tol<0)return 0;mulib_num_complex *m=(mulib_num_complex*)mulib_storage_alloc(nn,sizeof *m),*r=(mulib_num_complex*)mulib_storage_alloc(n,sizeof *r);if(!m||!r){free(m);free(r);return 0;}memcpy(m,a,nn*sizeof *m);memcpy(r,b,n*sizeof *r);int ok=1;for(size_t k=0;k<n;k++){size_t p=k;for(size_t i=k+1;i<n;i++)if(mulib_num_complex_abs(m[i*n+k])>mulib_num_complex_abs(m[p*n+k]))p=i;if(mulib_num_complex_abs(m[p*n+k])<=tol||!isfinite(mulib_num_complex_abs(m[p*n+k]))){ok=0;break;}if(p!=k){for(size_t j=k;j<n;j++){mulib_num_complex t=m[p*n+j];m[p*n+j]=m[k*n+j];m[k*n+j]=t;}mulib_num_complex t=r[p];r[p]=r[k];r[k]=t;}for(size_t i=k+1;i<n;i++){mulib_num_complex f=mulib_num_complex_div(m[i*n+k],m[k*n+k]);for(size_t j=k+1;j<n;j++)m[i*n+j]=mulib_num_complex_sub(m[i*n+j],mulib_num_complex_mul(f,m[k*n+j]));r[i]=mulib_num_complex_sub(r[i],mulib_num_complex_mul(f,r[k]));}}if(ok){for(size_t i=n;i-->0;){for(size_t j=i+1;j<n;j++)r[i]=mulib_num_complex_sub(r[i],mulib_num_complex_mul(m[i*n+j],r[j]));r[i]=mulib_num_complex_div(r[i],m[i*n+i]);if(!isfinite(r[i].re)||!isfinite(r[i].im))ok=0;}if(ok)memcpy(x,r,n*sizeof *r);}free(m);free(r);return ok;}
#ifdef __cplusplus
}
#endif
#endif
