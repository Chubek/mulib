/* MuLIB: pcb-footprint. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_FOOTPRINT in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_FOOTPRINT_H
#define MULIB_PCB_FOOTPRINT_H
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
#include "mulib-pcb-pad.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_pcb_footprint_bounds(const mulib_pcb_pad *pads,size_t n,mulib_geom2_box *out);
int mulib_pcb_footprint_unique(const mulib_pcb_pad *p,size_t n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_FOOTPRINT) && !defined(MULIB_PCB_FOOTPRINT_H_IMPLEMENTED)
#define MULIB_PCB_FOOTPRINT_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_footprint_bounds(const mulib_pcb_pad *pads,size_t n,mulib_geom2_box *out){if(!pads||!n||!out)return 0;mulib_geom2_box b=mulib_pcb_pad_bounds(pads);if(!mulib_geom2_box_valid(b))return 0;for(size_t i=1;i<n;i++){mulib_geom2_box x=mulib_pcb_pad_bounds(pads+i);if(!mulib_geom2_box_valid(x))return 0;b.lo.x=MULIB_STORAGE_MIN(b.lo.x,x.lo.x);b.lo.y=MULIB_STORAGE_MIN(b.lo.y,x.lo.y);b.hi.x=MULIB_STORAGE_MAX(b.hi.x,x.hi.x);b.hi.y=MULIB_STORAGE_MAX(b.hi.y,x.hi.y);}*out=b;return 1;}
int mulib_pcb_footprint_unique(const mulib_pcb_pad *p,size_t n){if(!p&&n)return 0;for(size_t i=0;i<n;i++){if(!p[i].id||!isfinite(p[i].width)||!isfinite(p[i].height)||p[i].width<=0||p[i].height<=0)return 0;for(size_t j=0;j<i;j++)if(p[j].id==p[i].id)return 0;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
