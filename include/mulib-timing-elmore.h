/* MuLIB / Amb3r: timing-elmore. C11 single-header kernel.
 * Define MULIB_IMPL_TIMING_ELMORE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_TIMING_ELMORE_H
#define MULIB_TIMING_ELMORE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_timing_elmore(size_t n,const size_t *parent,const double *resistance,const double *capacitance,size_t root,double *delay);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TIMING_ELMORE) && !defined(MULIB_TIMING_ELMORE_H_IMPLEMENTED)
#define MULIB_TIMING_ELMORE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_timing_elmore(size_t n,const size_t *parent,const double *resistance,const double *capacitance,size_t root,double *delay){if(!n||!parent||!resistance||!capacitance||!delay||root>=n||parent[root]!=MA_NONE)return 0;ma_edge *edges=(ma_edge*)ma_alloc(n-1,sizeof *edges);size_t *order=(size_t*)ma_alloc(n,sizeof *order);double *load=(double*)ma_alloc(n,sizeof *load);if(!edges||!order||!load){free(edges);free(order);free(load);return 0;}size_t k=0;int ok=0;for(size_t i=0;i<n;i++){if(capacitance[i]<0||resistance[i]<0||!isfinite(capacitance[i]+resistance[i])||(i!=root&&parent[i]>=n))goto end;load[i]=capacitance[i];if(i!=root)edges[k++]=(ma_edge){parent[i],i,resistance[i]};}if(!ma_topo(n,edges,k,order)||order[0]!=root)goto end;for(size_t i=n;i-->1;){size_t u=order[i];load[parent[u]]+=load[u];}delay[root]=0;for(size_t i=1;i<n;i++){size_t u=order[i];delay[u]=delay[parent[u]]+resistance[u]*load[u];}ok=ma_finite(delay,n);end:free(edges);free(order);free(load);return ok;}
#ifdef __cplusplus
}
#endif
#endif
