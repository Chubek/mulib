/* MuLIB: ecad-instance. C11 single-header kernel.
 * Define MULIB_IMPL_ECAD_INSTANCE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_ECAD_INSTANCE_H
#define MULIB_ECAD_INSTANCE_H
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

mulib_geom2_point mulib_ecad_instance_transform(mulib_geom2_point p,mulib_geom2_point offset,unsigned rotation,int mirror);
mulib_geom2_point mulib_ecad_instance_inverse(mulib_geom2_point p,mulib_geom2_point offset,unsigned rotation,int mirror);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ECAD_INSTANCE) && !defined(MULIB_ECAD_INSTANCE_H_IMPLEMENTED)
#define MULIB_ECAD_INSTANCE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
mulib_geom2_point mulib_ecad_instance_transform(mulib_geom2_point p,mulib_geom2_point offset,unsigned rotation,int mirror){if(mirror)p.x=-p.x;for(unsigned i=0;i<rotation%4;i++){double t=p.x;p.x=-p.y;p.y=t;}p.x+=offset.x;p.y+=offset.y;return p;}
mulib_geom2_point mulib_ecad_instance_inverse(mulib_geom2_point p,mulib_geom2_point offset,unsigned rotation,int mirror){p.x-=offset.x;p.y-=offset.y;for(unsigned i=0;i<(4-rotation%4)%4;i++){double t=p.x;p.x=-p.y;p.y=t;}if(mirror)p.x=-p.x;return p;}
#ifdef __cplusplus
}
#endif
#endif
