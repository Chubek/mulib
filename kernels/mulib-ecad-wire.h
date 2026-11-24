/* MuLIB: ecad-wire. C11 single-header kernel.
 * Define MULIB_IMPL_ECAD_WIRE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_ECAD_WIRE_H
#define MULIB_ECAD_WIRE_H
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

int mulib_ecad_wire_orthogonal(mulib_geom2_point a,mulib_geom2_point b,mulib_geom2_point *out,size_t cap,size_t *count);
int mulib_ecad_wire_contains(mulib_geom2_point a,mulib_geom2_point b,mulib_geom2_point p,double epsilon);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ECAD_WIRE) && !defined(MULIB_ECAD_WIRE_H_IMPLEMENTED)
#define MULIB_ECAD_WIRE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ecad_wire_orthogonal(mulib_geom2_point a,mulib_geom2_point b,mulib_geom2_point *out,size_t cap,size_t *count){if(!count||!mulib_geom2_point_valid(a)||!mulib_geom2_point_valid(b))return 0;*count=(a.x==b.x||a.y==b.y)?2:3;if(!out||cap<*count)return 0;out[0]=a;if(*count==3){out[1].x=b.x;out[1].y=a.y;}out[*count-1]=b;return 1;}
int mulib_ecad_wire_contains(mulib_geom2_point a,mulib_geom2_point b,mulib_geom2_point p,double epsilon){return epsilon>=0&&mulib_geom2_segment_distance(p,a,b)<=epsilon;}
#ifdef __cplusplus
}
#endif
#endif
