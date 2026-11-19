/* MuLIB / Amb3r: pcb-router. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_ROUTER in exactly one implementation translation unit.
 * Profile: Four-neighbor obstacle-grid A*; not a multilayer manufacturing-aware router.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_ROUTER_H
#define MULIB_PCB_ROUTER_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_pcb_router(size_t width,size_t height,const unsigned char *blocked,size_t start,size_t goal,size_t *path,size_t capacity,size_t *length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_ROUTER) && !defined(MULIB_PCB_ROUTER_H_IMPLEMENTED)
#define MULIB_PCB_ROUTER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_router(size_t width,size_t height,const unsigned char *blocked,size_t start,size_t goal,size_t *path,size_t capacity,size_t *length){return ma_grid_route(width,height,blocked,start,goal,path,capacity,length);}
#ifdef __cplusplus
}
#endif
#endif
