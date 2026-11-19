/* MuLIB / Amb3r: ecad-wire-route. C11 single-header kernel.
 * Define MULIB_IMPL_ECAD_WIRE_ROUTE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_ECAD_WIRE_ROUTE_H
#define MULIB_ECAD_WIRE_ROUTE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_ecad_wire_route(size_t width,size_t height,const unsigned char *blocked,size_t start,size_t goal,size_t *path,size_t capacity,size_t *length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ECAD_WIRE_ROUTE) && !defined(MULIB_ECAD_WIRE_ROUTE_H_IMPLEMENTED)
#define MULIB_ECAD_WIRE_ROUTE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ecad_wire_route(size_t width,size_t height,const unsigned char *blocked,size_t start,size_t goal,size_t *path,size_t capacity,size_t *length){return ma_grid_route(width,height,blocked,start,goal,path,capacity,length);}
#ifdef __cplusplus
}
#endif
#endif
