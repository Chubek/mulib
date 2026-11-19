/* MuLIB / Amb3r: eda-dependency. C11 single-header kernel.
 * Define MULIB_IMPL_EDA_DEPENDENCY in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_EDA_DEPENDENCY_H
#define MULIB_EDA_DEPENDENCY_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_eda_dependency_order(size_t n,const ma_edge *edges,size_t m,size_t *order);
int mulib_eda_dependency_dirty(size_t n,const ma_edge *e,size_t m,size_t root,unsigned char *dirty);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDA_DEPENDENCY) && !defined(MULIB_EDA_DEPENDENCY_H_IMPLEMENTED)
#define MULIB_EDA_DEPENDENCY_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_dependency_order(size_t n,const ma_edge *edges,size_t m,size_t *order){return ma_topo(n,edges,m,order);}
int mulib_eda_dependency_dirty(size_t n,const ma_edge *e,size_t m,size_t root,unsigned char *dirty){if(root>=n||!dirty||(!e&&m))return 0;for(size_t i=0;i<m;i++)if(e[i].from>=n||e[i].to>=n)return 0;memset(dirty,0,n);dirty[root]=1;int change=1;while(change){change=0;for(size_t i=0;i<m;i++)if(dirty[e[i].from]&&!dirty[e[i].to]){dirty[e[i].to]=1;change=1;}}return 1;}
#ifdef __cplusplus
}
#endif
#endif
