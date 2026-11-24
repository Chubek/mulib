/* MuLIB: num-sparse-lu. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_SPARSE_LU in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_SPARSE_LU_H
#define MULIB_NUM_SPARSE_LU_H
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
typedef struct {size_t col;double v;} mulib_num_lu_entry;
typedef struct {mulib_num_lu_entry *v;size_t n,cap;} mulib_num_lu_row;
typedef struct {mulib_num_lu_row *rows;size_t *piv,n;} mulib_num_sparse_lu;
static inline double mulib_num_lu_get(const mulib_num_lu_row *r,size_t c){for(size_t i=0;i<r->n;i++)if(r->v[i].col==c)return r->v[i].v;return 0;}
static inline int mulib_num_lu_set(mulib_num_lu_row *r,size_t c,double v){if(!isfinite(v))return 0;for(size_t i=0;i<r->n;i++)if(r->v[i].col==c){if(v==0)r->v[i]=r->v[--r->n];else r->v[i].v=v;return 1;}mulib_num_lu_entry e={c,v};return v==0||mulib_storage_push((void**)&r->v,&r->n,&r->cap,sizeof e,&e);}
int mulib_num_sparse_lu_factor(const mulib_num_csr_view *a,double tolerance,mulib_num_sparse_lu *out);
int mulib_num_sparse_lu_solve(const mulib_num_sparse_lu *lu,const double *b,double *x);
void mulib_num_sparse_lu_free(mulib_num_sparse_lu *lu);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_SPARSE_LU) && !defined(MULIB_NUM_SPARSE_LU_H_IMPLEMENTED)
#define MULIB_NUM_SPARSE_LU_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_sparse_lu_factor(const mulib_num_csr_view *a,double tolerance,mulib_num_sparse_lu *out){if(!mulib_num_csr_view_valid(a)||a->rows!=a->cols||!a->rows||!out||out->rows||out->piv||tolerance<0||!isfinite(tolerance))return 0;size_t n=a->rows;mulib_num_sparse_lu t={0};t.n=n;t.rows=(mulib_num_lu_row*)mulib_storage_alloc(n,sizeof *t.rows);t.piv=(size_t*)mulib_storage_alloc(n,sizeof *t.piv);if(!t.rows||!t.piv)goto fail;for(size_t i=0;i<n;i++){t.piv[i]=i;for(size_t k=a->ptr[i];k<a->ptr[i+1];k++)if(!mulib_num_lu_set(t.rows+i,a->idx[k],mulib_num_lu_get(t.rows+i,a->idx[k])+a->val[k]))goto fail;}for(size_t k=0;k<n;k++){size_t p=k;for(size_t i=k+1;i<n;i++)if(fabs(mulib_num_lu_get(t.rows+i,k))>fabs(mulib_num_lu_get(t.rows+p,k)))p=i;double d=mulib_num_lu_get(t.rows+p,k);if(!isfinite(d)||fabs(d)<=tolerance)goto fail;if(p!=k){mulib_num_lu_row r=t.rows[p];t.rows[p]=t.rows[k];t.rows[k]=r;size_t q=t.piv[p];t.piv[p]=t.piv[k];t.piv[k]=q;}for(size_t i=k+1;i<n;i++){double x=mulib_num_lu_get(t.rows+i,k);if(x==0)continue;double f=x/d;if(!mulib_num_lu_set(t.rows+i,k,f))goto fail;for(size_t j=0;j<t.rows[k].n;j++){size_t c=t.rows[k].v[j].col;if(c>k&&!mulib_num_lu_set(t.rows+i,c,mulib_num_lu_get(t.rows+i,c)-f*t.rows[k].v[j].v))goto fail;}}}*out=t;return 1;fail:mulib_num_sparse_lu_free(&t);return 0;}
int mulib_num_sparse_lu_solve(const mulib_num_sparse_lu *lu,const double *b,double *x){if(!lu||!lu->n||!lu->rows||!lu->piv||!b||!x||!mulib_num_all_finite(b,lu->n))return 0;size_t n=lu->n;double *r=(double*)mulib_storage_alloc(n,sizeof(double));if(!r)return 0;for(size_t i=0;i<n;i++){r[i]=b[lu->piv[i]];for(size_t k=0;k<lu->rows[i].n;k++){mulib_num_lu_entry e=lu->rows[i].v[k];if(e.col<i)r[i]-=e.v*r[e.col];}}for(size_t i=n;i-->0;){for(size_t k=0;k<lu->rows[i].n;k++){mulib_num_lu_entry e=lu->rows[i].v[k];if(e.col>i)r[i]-=e.v*r[e.col];}r[i]/=mulib_num_lu_get(lu->rows+i,i);}int ok=mulib_num_all_finite(r,n);if(ok)memcpy(x,r,n*sizeof(double));free(r);return ok;}
void mulib_num_sparse_lu_free(mulib_num_sparse_lu *lu){if(lu){if(lu->rows)for(size_t i=0;i<lu->n;i++)free(lu->rows[i].v);free(lu->rows);free(lu->piv);memset(lu,0,sizeof *lu);}}
#ifdef __cplusplus
}
#endif
#endif
