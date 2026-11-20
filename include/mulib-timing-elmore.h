/* MuLIB: timing-elmore. C11 single-header kernel.
 * Define MULIB_IMPL_TIMING_ELMORE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_TIMING_ELMORE_H
#define MULIB_TIMING_ELMORE_H
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
int mulib_timing_elmore(size_t n,const size_t *parent,const double *resistance,const double *capacitance,size_t root,double *delay){if(!n||!parent||!resistance||!capacitance||!delay||root>=n||parent[root]!=MULIB_STORAGE_NONE)return 0;mulib_graph_weighted_edge *edges=(mulib_graph_weighted_edge*)mulib_storage_alloc(n-1,sizeof *edges);size_t *order=(size_t*)mulib_storage_alloc(n,sizeof *order);double *load=(double*)mulib_storage_alloc(n,sizeof *load);if(!edges||!order||!load){free(edges);free(order);free(load);return 0;}size_t k=0;int ok=0;for(size_t i=0;i<n;i++){if(capacitance[i]<0||resistance[i]<0||!isfinite(capacitance[i]+resistance[i])||(i!=root&&parent[i]>=n))goto end;load[i]=capacitance[i];if(i!=root)edges[k++]=(mulib_graph_weighted_edge){parent[i],i,resistance[i]};}if(!mulib_graph_edges_toposort(n,edges,k,order)||order[0]!=root)goto end;for(size_t i=n;i-->1;){size_t u=order[i];load[parent[u]]+=load[u];}delay[root]=0;for(size_t i=1;i<n;i++){size_t u=order[i];delay[u]=delay[parent[u]]+resistance[u]*load[u];}ok=mulib_num_all_finite(delay,n);end:free(edges);free(order);free(load);return ok;}
#ifdef __cplusplus
}
#endif
#endif
