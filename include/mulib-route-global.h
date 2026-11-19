/* MuLIB / Amb3r: route-global. C11 single-header kernel.
 * Define MULIB_IMPL_ROUTE_GLOBAL in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_ROUTE_GLOBAL_H
#define MULIB_ROUTE_GLOBAL_H
#include "mulib-amb3r-base.h"
#include "mulib-formal-counterexample.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_route_global(size_t n,const ma_edge *edges,size_t m,size_t source,size_t target,size_t *path,size_t cap,size_t *length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ROUTE_GLOBAL) && !defined(MULIB_ROUTE_GLOBAL_H_IMPLEMENTED)
#define MULIB_ROUTE_GLOBAL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_route_global(size_t n,const ma_edge *edges,size_t m,size_t source,size_t target,size_t *path,size_t cap,size_t *length){if(target>=n||source>=n||!length)return 0;double *d=(double*)ma_alloc(n,sizeof *d);size_t *p=(size_t*)ma_alloc(n,sizeof *p);if(!d||!p){free(d);free(p);return 0;}int ok=ma_dijkstra(n,edges,m,source,d,p)&&isfinite(d[target])&&mulib_formal_counterexample(p,n,target,path,cap,length);free(d);free(p);return ok;}
#ifdef __cplusplus
}
#endif
#endif
