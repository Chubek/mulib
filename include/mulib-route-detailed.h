/* MuLIB / Amb3r: route-detailed. C11 single-header kernel.
 * Define MULIB_IMPL_ROUTE_DETAILED in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_ROUTE_DETAILED_H
#define MULIB_ROUTE_DETAILED_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_route_detailed(size_t width,size_t height,const unsigned char *blocked,size_t start,size_t goal,size_t *path,size_t capacity,size_t *length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ROUTE_DETAILED) && !defined(MULIB_ROUTE_DETAILED_H_IMPLEMENTED)
#define MULIB_ROUTE_DETAILED_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_route_detailed(size_t width,size_t height,const unsigned char *blocked,size_t start,size_t goal,size_t *path,size_t capacity,size_t *length){return ma_grid_route(width,height,blocked,start,goal,path,capacity,length);}
#ifdef __cplusplus
}
#endif
#endif
