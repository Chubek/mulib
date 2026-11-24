/* MuLIB: pcb-placement. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_PLACEMENT in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_PLACEMENT_H
#define MULIB_PCB_PLACEMENT_H
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
#include "mulib-geom2-core.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_pcb_placement_shelf(const mulib_geom2_point *sizes,size_t n,double width,double gap,mulib_geom2_point *positions,double *height);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_PLACEMENT) && !defined(MULIB_PCB_PLACEMENT_H_IMPLEMENTED)
#define MULIB_PCB_PLACEMENT_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_placement_shelf(const mulib_geom2_point *sizes,size_t n,double width,double gap,mulib_geom2_point *positions,double *height){if((!sizes&&n)||(!positions&&n)||!height||width<=0||gap<0)return 0;double x=0,y=0,row=0;for(size_t i=0;i<n;i++){if(!mulib_geom2_point_valid(sizes[i])||sizes[i].x<=0||sizes[i].y<=0||sizes[i].x>width)return 0;if(x+sizes[i].x>width){x=0;y+=row+gap;row=0;}positions[i]=(mulib_geom2_point){x,y};x+=sizes[i].x+gap;row=MULIB_STORAGE_MAX(row,sizes[i].y);}*height=y+row;return 1;}
#ifdef __cplusplus
}
#endif
#endif
