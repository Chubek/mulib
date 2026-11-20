/* MuLIB: ecad-snap. C11 single-header kernel.
 * Define MULIB_IMPL_ECAD_SNAP in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_ECAD_SNAP_H
#define MULIB_ECAD_SNAP_H
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

int mulib_ecad_snap(mulib_geom2_point input,double spacing,const mulib_geom2_point *targets,size_t n,double radius,mulib_geom2_point *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ECAD_SNAP) && !defined(MULIB_ECAD_SNAP_H_IMPLEMENTED)
#define MULIB_ECAD_SNAP_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ecad_snap(mulib_geom2_point input,double spacing,const mulib_geom2_point *targets,size_t n,double radius,mulib_geom2_point *out){if(!out||!mulib_geom2_point_valid(input)||!isfinite(spacing)||spacing<=0||radius<0||(!targets&&n))return 0;*out=(mulib_geom2_point){round(input.x/spacing)*spacing,round(input.y/spacing)*spacing};double best=radius;for(size_t i=0;i<n;i++){double d=mulib_geom2_distance(input,targets[i]);if(d<=best){*out=targets[i];best=d;}}return mulib_geom2_point_valid(*out);}
#ifdef __cplusplus
}
#endif
#endif
