/* MuLIB / Amb3r: route-negotiated. C11 single-header kernel.
 * Define MULIB_IMPL_ROUTE_NEGOTIATED in exactly one implementation translation unit.
 * Profile: One-commodity route with congestion costs; iterative driver in fpga-pathfinder.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_ROUTE_NEGOTIATED_H
#define MULIB_ROUTE_NEGOTIATED_H
#include "mulib-amb3r-base.h"
#include "mulib-route-global.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_route_negotiated(size_t vertices,const ma_edge *edges,size_t n,const unsigned *usage,const unsigned *capacity,const double *history,size_t source,size_t target,size_t *path,size_t cap,size_t *length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ROUTE_NEGOTIATED) && !defined(MULIB_ROUTE_NEGOTIATED_H_IMPLEMENTED)
#define MULIB_ROUTE_NEGOTIATED_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_route_negotiated(size_t vertices,const ma_edge *edges,size_t n,const unsigned *usage,const unsigned *capacity,const double *history,size_t source,size_t target,size_t *path,size_t cap,size_t *length){if((!edges&&n)||(!usage&&n)||(!capacity&&n)||(!history&&n))return 0;ma_edge *e=(ma_edge*)ma_alloc(n,sizeof *e);if(!e)return 0;for(size_t i=0;i<n;i++){if(!capacity[i]||history[i]<0||!isfinite(history[i])){free(e);return 0;}e[i]=edges[i];double overflow=usage[i]>=capacity[i]?(double)usage[i]-capacity[i]+1:0;e[i].weight=edges[i].weight*(1+overflow)+history[i];}int ok=mulib_route_global(vertices,e,n,source,target,path,cap,length);free(e);return ok;}
#ifdef __cplusplus
}
#endif
#endif
