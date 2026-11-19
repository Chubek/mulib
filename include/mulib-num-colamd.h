/* MuLIB / Amb3r: num-colamd. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_COLAMD in exactly one implementation translation unit.
 * Profile: Explicit column intersection graph minimum-degree ordering; not the COLAMD quotient-graph algorithm.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_COLAMD_H
#define MULIB_NUM_COLAMD_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_num_colamd(const ma_csr *a,size_t *order);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_COLAMD) && !defined(MULIB_NUM_COLAMD_H_IMPLEMENTED)
#define MULIB_NUM_COLAMD_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_colamd(const ma_csr *a,size_t *order){if(!ma_csr_valid(a)||!order)return 0;size_t n=a->cols,nn;if(!ma_mul(n,n,&nn))return 0;unsigned char *g=(unsigned char*)ma_alloc(nn,1),*done=(unsigned char*)ma_alloc(n,1);if(!g||!done){free(g);free(done);return 0;}for(size_t i=0;i<a->rows;i++)for(size_t j=a->ptr[i];j<a->ptr[i+1];j++)for(size_t k=j+1;k<a->ptr[i+1];k++){size_t u=a->idx[j],v=a->idx[k];if(u!=v)g[u*n+v]=g[v*n+u]=1;}for(size_t k=0;k<n;k++){size_t u=MA_NONE,best=MA_NONE;for(size_t i=0;i<n;i++)if(!done[i]){size_t d=0;for(size_t j=0;j<n;j++)if(!done[j]&&g[i*n+j])d++;if(d<best){best=d;u=i;}}order[k]=u;done[u]=1;for(size_t i=0;i<n;i++)if(!done[i]&&g[u*n+i])for(size_t j=i+1;j<n;j++)if(!done[j]&&g[u*n+j])g[i*n+j]=g[j*n+i]=1;}free(g);free(done);return 1;}
#ifdef __cplusplus
}
#endif
#endif
