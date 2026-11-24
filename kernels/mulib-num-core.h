/** @file mulib-num-core.h
 * @brief Double vector, complex, borrowed CSR and reference solve primitives.
 * C11 header-only substrate; standard library and libm only.
 * Pointer/count arguments require live arrays of the declared lengths.
 * Stateful objects require caller synchronization.
 */
#ifndef MULIB_NUM_CORE_H
#define MULIB_NUM_CORE_H
#include "mulib-storage-core.h"
#include <math.h>
#include <float.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Complex components are re/im doubles; vectors and CSR arrays are borrowed
 * unless explicitly allocated by the caller. CSR indices/counts use size_t.
 * The vector descriptor does not imply ownership or an automatic release.
 * Dense solve copies inputs and writes x only on success; matvec requires
 * disjoint input/output arrays. The legacy logical CSR validator cannot
 * verify physical capacities: callers supply rows+1 offsets and nnz entries.
 * Arithmetic preserves the original IEEE behavior, including nonfinite
 * results on overflow and division by zero; no implicit sanitization.
 * Dot/norm take n readable doubles (both dot inputs); they do not validate
 * pointers or finiteness. RNG requires writable state; zero seeds are mapped
 * to a fixed nonzero seed, and uniform results are in [0,1).
 * CSR matvec requires cols readable x and rows writable y elements.
 * Dense solve requires n*n readable a, n readable b and n writable x doubles;
 * n*n allocation is checked and the tolerance must be finite/nonnegative. */
#define MULIB_NUM_PI 3.141592653589793238462643383279502884
typedef struct {double re,im;} mulib_num_complex;
/* Borrowed CSR: rows+1 offsets and nnz column indices/values, all in elements.
 * Indices are zero-based; ordering, duplicate entries, and explicit zeros are preserved. */
typedef struct {size_t rows,cols,nnz; const size_t *ptr,*idx; const double *val;} mulib_num_csr_view;
typedef struct {double *v; size_t n,cap;} mulib_num_vector;
typedef struct {uint64_t state;} mulib_num_rng;
static inline int mulib_num_all_finite(const double *v,size_t n){if(!v&&n)return 0;for(size_t i=0;i<n;i++)if(!isfinite(v[i]))return 0;return 1;}
static inline double mulib_num_dot(const double *a,const double *b,size_t n){double s=0;for(size_t i=0;i<n;i++)s+=a[i]*b[i];return s;}
static inline double mulib_num_norm(const double *v,size_t n){double s=0;for(size_t i=0;i<n;i++)s=hypot(s,v[i]);return s;}
static inline uint64_t mulib_num_rng_next(mulib_num_rng *r){uint64_t x=r->state?r->state:UINT64_C(0x9e3779b97f4a7c15);x^=x>>12;x^=x<<25;x^=x>>27;r->state=x;return x*UINT64_C(2685821657736338717);}
static inline double mulib_num_rng_uniform(mulib_num_rng *r){return (double)(mulib_num_rng_next(r)>>11)*0x1p-53;}
static inline mulib_num_complex mulib_num_complex_add(mulib_num_complex a,mulib_num_complex b){mulib_num_complex z={a.re+b.re,a.im+b.im};return z;}
static inline mulib_num_complex mulib_num_complex_sub(mulib_num_complex a,mulib_num_complex b){mulib_num_complex z={a.re-b.re,a.im-b.im};return z;}
static inline mulib_num_complex mulib_num_complex_mul(mulib_num_complex a,mulib_num_complex b){mulib_num_complex z={a.re*b.re-a.im*b.im,a.re*b.im+a.im*b.re};return z;}
static inline mulib_num_complex mulib_num_complex_div(mulib_num_complex a,mulib_num_complex b){double d=b.re*b.re+b.im*b.im;mulib_num_complex z={(a.re*b.re+a.im*b.im)/d,(a.im*b.re-a.re*b.im)/d};return z;}
static inline double mulib_num_complex_abs(mulib_num_complex a){return hypot(a.re,a.im);}
/* Legacy logical validator: caller must supply live arrays of the declared sizes. */
static inline int mulib_num_csr_view_valid(const mulib_num_csr_view *a) {
 if(!a||a->rows>SIZE_MAX/sizeof(size_t)-1||a->nnz>SIZE_MAX/sizeof(size_t)||
    a->nnz>SIZE_MAX/sizeof(double)||!a->ptr||(a->nnz&&(!a->idx||!a->val))||
    a->ptr[0]||a->ptr[a->rows]!=a->nnz)return 0;
 for(size_t i=0;i<a->rows;i++)
  if(a->ptr[i]>a->ptr[i+1]||a->ptr[i+1]>a->nnz)return 0;
 for(size_t k=0;k<a->nnz;k++)
  if(a->idx[k]>=a->cols||!isfinite(a->val[k]))return 0;
 return 1;
}
static inline void mulib_num_csr_view_matvec(const mulib_num_csr_view *a,const double *x,double *y){for(size_t i=0;i<a->rows;i++){double s=0;for(size_t k=a->ptr[i];k<a->ptr[i+1];k++)s+=a->val[k]*x[a->idx[k]];y[i]=s;}}
/* Dense reference solve, copied inputs; used only by bounded reference profiles. */
static inline int mulib_num_dense_reference_solve(size_t n,const double *a,const double *b,double *x,double tol){
 size_t nn;if(!n||!a||!b||!x||!isfinite(tol)||tol<0||!mulib_storage_size_mul(n,n,&nn)||!mulib_num_all_finite(a,nn)||!mulib_num_all_finite(b,n))return 0;
 double *m=(double*)mulib_storage_alloc(nn,sizeof(double)),*r=(double*)mulib_storage_alloc(n,sizeof(double));if(!m||!r){free(m);free(r);return 0;}memcpy(m,a,nn*sizeof(double));memcpy(r,b,n*sizeof(double));int ok=1;
 for(size_t k=0;k<n;k++){size_t p=k;for(size_t i=k+1;i<n;i++)if(fabs(m[i*n+k])>fabs(m[p*n+k]))p=i;if(fabs(m[p*n+k])<=tol){ok=0;break;}if(p!=k){for(size_t j=k;j<n;j++){double t=m[k*n+j];m[k*n+j]=m[p*n+j];m[p*n+j]=t;}double t=r[k];r[k]=r[p];r[p]=t;}for(size_t i=k+1;i<n;i++){double f=m[i*n+k]/m[k*n+k];for(size_t j=k+1;j<n;j++)m[i*n+j]-=f*m[k*n+j];r[i]-=f*r[k];}}
 if(ok){for(size_t i=n;i-->0;){double v=r[i];for(size_t j=i+1;j<n;j++)v-=m[i*n+j]*r[j];r[i]=v/m[i*n+i];}ok=mulib_num_all_finite(r,n);if(ok)memcpy(x,r,n*sizeof(double));}free(m);free(r);return ok;}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_NUM_CORE_H */
