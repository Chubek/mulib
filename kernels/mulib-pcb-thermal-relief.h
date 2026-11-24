/* MuLIB: pcb-thermal-relief. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_THERMAL_RELIEF in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_THERMAL_RELIEF_H
#define MULIB_PCB_THERMAL_RELIEF_H
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
#include "mulib-num-core.h"
#include "mulib-geom2-core.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_pcb_thermal_relief(mulib_geom2_point center,double inner,double outer,unsigned spokes,double angle,mulib_geom2_point *segments,size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_THERMAL_RELIEF) && !defined(MULIB_PCB_THERMAL_RELIEF_H_IMPLEMENTED)
#define MULIB_PCB_THERMAL_RELIEF_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_thermal_relief(mulib_geom2_point center,double inner,double outer,unsigned spokes,double angle,mulib_geom2_point *segments,size_t capacity){if(!segments||!spokes||spokes>capacity/2||inner<0||outer<=inner||!isfinite(angle)||!mulib_geom2_point_valid(center))return 0;for(unsigned i=0;i<spokes;i++){double t=angle+2*MULIB_NUM_PI*i/spokes;segments[2*i]=(mulib_geom2_point){center.x+inner*cos(t),center.y+inner*sin(t)};segments[2*i+1]=(mulib_geom2_point){center.x+outer*cos(t),center.y+outer*sin(t)};}return 1;}
#ifdef __cplusplus
}
#endif
#endif
