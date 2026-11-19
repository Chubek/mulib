/* MuLIB / Amb3r: num-symbolic. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_SYMBOLIC in exactly one implementation translation unit.
 * Profile: Symbolic LU fill pattern for fixed row/column ordering; pivoting may introduce extra fill.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_SYMBOLIC_H
#define MULIB_NUM_SYMBOLIC_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_num_symbolic_lu(const ma_csr *a,size_t *row_counts,size_t *nnz);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_SYMBOLIC) && !defined(MULIB_NUM_SYMBOLIC_H_IMPLEMENTED)
#define MULIB_NUM_SYMBOLIC_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_symbolic_lu(const ma_csr *a,size_t *row_counts,size_t *nnz){if(!ma_csr_valid(a)||a->rows!=a->cols||!row_counts||!nnz)return 0;size_t n=a->rows,nn;if(!ma_mul(n,n,&nn))return 0;unsigned char *g=(unsigned char*)ma_alloc(nn,1);if(!g)return 0;for(size_t i=0;i<n;i++){g[i*n+i]=1;for(size_t k=a->ptr[i];k<a->ptr[i+1];k++)g[i*n+a->idx[k]]=1;}for(size_t k=0;k<n;k++)for(size_t i=k+1;i<n;i++)if(g[i*n+k])for(size_t j=k+1;j<n;j++)if(g[k*n+j])g[i*n+j]=1;*nnz=0;for(size_t i=0;i<n;i++){row_counts[i]=0;for(size_t j=0;j<n;j++)row_counts[i]+=g[i*n+j]!=0;*nnz+=row_counts[i];}free(g);return 1;}
#ifdef __cplusplus
}
#endif
#endif
