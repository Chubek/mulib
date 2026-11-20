/* MuLIB: pcb-lengthmatch. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_LENGTHMATCH in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_LENGTHMATCH_H
#define MULIB_PCB_LENGTHMATCH_H
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
#include "mulib-geom2-core.h"
#ifdef __cplusplus
extern "C" {
#endif

double mulib_pcb_length(const mulib_geom2_point *p,size_t n);
int mulib_pcb_lengthmatch_serpentine(mulib_geom2_point a,mulib_geom2_point b,double target,size_t teeth,mulib_geom2_point *out,size_t cap,size_t *n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_LENGTHMATCH) && !defined(MULIB_PCB_LENGTHMATCH_H_IMPLEMENTED)
#define MULIB_PCB_LENGTHMATCH_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
double mulib_pcb_length(const mulib_geom2_point *p,size_t n){if(!p&&n)return NAN;double d=0;for(size_t i=1;i<n;i++)d+=mulib_geom2_distance(p[i-1],p[i]);return d;}
int mulib_pcb_lengthmatch_serpentine(mulib_geom2_point a,mulib_geom2_point b,double target,size_t teeth,mulib_geom2_point *out,size_t cap,size_t *n){if(!n||!teeth||teeth>(SIZE_MAX-2)/4||!isfinite(target))return 0;double dx=b.x-a.x,dy=b.y-a.y,d=hypot(dx,dy);if(!d||target<d)return 0;*n=4*teeth+2;if(!out||cap<*n)return 0;double amp=(target-d)/(2*teeth),ux=dx/d,uy=dy/d;out[0]=a;for(size_t i=0;i<teeth;i++){double t0=d*(i+.25)/teeth,t1=d*(i+.75)/teeth;out[4*i+1]=(mulib_geom2_point){a.x+ux*t0,a.y+uy*t0};out[4*i+2]=(mulib_geom2_point){a.x+ux*t0-uy*amp,a.y+uy*t0+ux*amp};out[4*i+3]=(mulib_geom2_point){a.x+ux*t1-uy*amp,a.y+uy*t1+ux*amp};out[4*i+4]=(mulib_geom2_point){a.x+ux*t1,a.y+uy*t1};}out[*n-1]=b;return 1;}
#ifdef __cplusplus
}
#endif
#endif
