/* MuLIB: pcb-via. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_VIA in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_VIA_H
#define MULIB_PCB_VIA_H
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
typedef struct {mulib_geom2_point center;double drill,diameter;unsigned first_layer,last_layer;uint64_t net;} mulib_pcb_via;
int mulib_pcb_via_valid(const mulib_pcb_via *v,unsigned layers,double min_ring);
double mulib_pcb_via_resistance(double rho,double length,double drill,double plating);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_VIA) && !defined(MULIB_PCB_VIA_H_IMPLEMENTED)
#define MULIB_PCB_VIA_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_via_valid(const mulib_pcb_via *v,unsigned layers,double min_ring){return v&&mulib_geom2_point_valid(v->center)&&v->first_layer<v->last_layer&&v->last_layer<layers&&isfinite(v->drill)&&isfinite(v->diameter)&&v->drill>0&&min_ring>=0&&v->diameter-v->drill>=2*min_ring;}
double mulib_pcb_via_resistance(double rho,double length,double drill,double plating){if(rho<0||length<0||drill<=0||plating<=0)return NAN;return rho*length/(MULIB_NUM_PI*plating*(drill+plating));}
#ifdef __cplusplus
}
#endif
#endif
