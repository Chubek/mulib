/* MuLIB: pcb-panelize. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_PANELIZE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_PANELIZE_H
#define MULIB_PCB_PANELIZE_H
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
#include "mulib-geom2-core.h"
#include "mulib-graph-core.h"
#ifdef __cplusplus
extern "C" {
#endif

size_t mulib_pcb_panelize(double board_w,double board_h,double panel_w,double panel_h,double gap,mulib_geom2_point *out,size_t cap);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_PANELIZE) && !defined(MULIB_PCB_PANELIZE_H_IMPLEMENTED)
#define MULIB_PCB_PANELIZE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_pcb_panelize(double board_w,double board_h,double panel_w,double panel_h,double gap,mulib_geom2_point *out,size_t cap){if(board_w<=0||board_h<=0||panel_w<=0||panel_h<=0||gap<0||!isfinite(board_w+board_h+panel_w+panel_h+gap))return MULIB_STORAGE_NONE;double nx=floor((panel_w+gap)/(board_w+gap)),ny=floor((panel_h+gap)/(board_h+gap));if(nx>=(double)SIZE_MAX||ny>=(double)SIZE_MAX)return MULIB_STORAGE_NONE;size_t w=(size_t)nx,h=(size_t)ny,n;if(!mulib_storage_size_mul(w,h,&n))return MULIB_STORAGE_NONE;for(size_t i=0;out&&i<n&&i<cap;i++)out[i]=(mulib_geom2_point){(i%w)*(board_w+gap),(i/w)*(board_h+gap)};return n;}
#ifdef __cplusplus
}
#endif
#endif
