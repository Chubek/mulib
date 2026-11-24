/* MuLIB: num-amd. C11 single-header kernel.
 * Define MULIB_IMPL_NUM_AMD in exactly one implementation translation unit.
 * Profile: Explicit fill-graph minimum-degree ordering; not SuiteSparse AMD or industrial sparse ordering.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_NUM_AMD_H
#define MULIB_NUM_AMD_H
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

int mulib_num_amd(const mulib_num_csr_view *a,size_t *order);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NUM_AMD) && !defined(MULIB_NUM_AMD_H_IMPLEMENTED)
#define MULIB_NUM_AMD_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_num_amd(const mulib_num_csr_view *a,size_t *order){if(!mulib_num_csr_view_valid(a)||a->rows!=a->cols||!order)return 0;size_t n=a->rows,nn;if(!mulib_storage_size_mul(n,n,&nn))return 0;unsigned char *g=(unsigned char*)mulib_storage_alloc(nn,1),*done=(unsigned char*)mulib_storage_alloc(n,1);if(!g||!done){free(g);free(done);return 0;}for(size_t i=0;i<n;i++)for(size_t k=a->ptr[i];k<a->ptr[i+1];k++){size_t j=a->idx[k];if(i!=j)g[i*n+j]=g[j*n+i]=1;}for(size_t k=0;k<n;k++){size_t best=MULIB_STORAGE_NONE,deg=MULIB_STORAGE_NONE;for(size_t i=0;i<n;i++)if(!done[i]){size_t d=0;for(size_t j=0;j<n;j++)if(!done[j]&&g[i*n+j])d++;if(d<deg){best=i;deg=d;}}order[k]=best;done[best]=1;for(size_t i=0;i<n;i++)if(!done[i]&&g[best*n+i])for(size_t j=i+1;j<n;j++)if(!done[j]&&g[best*n+j])g[i*n+j]=g[j*n+i]=1;}free(g);free(done);return 1;}
#ifdef __cplusplus
}
#endif
#endif
