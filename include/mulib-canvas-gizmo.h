/* MuLIB: canvas-gizmo. C11 single-header kernel.
 * Define MULIB_IMPL_CANVAS_GIZMO in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_CANVAS_GIZMO_H
#define MULIB_CANVAS_GIZMO_H
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

mulib_geom2_point mulib_canvas_gizmo_rotate(mulib_geom2_point p,mulib_geom2_point center,double angle);
int mulib_canvas_gizmo_scale(mulib_geom2_point p,mulib_geom2_point anchor,mulib_geom2_point scale,mulib_geom2_point *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CANVAS_GIZMO) && !defined(MULIB_CANVAS_GIZMO_H_IMPLEMENTED)
#define MULIB_CANVAS_GIZMO_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
mulib_geom2_point mulib_canvas_gizmo_rotate(mulib_geom2_point p,mulib_geom2_point center,double angle){double x=p.x-center.x,y=p.y-center.y,c=cos(angle),s=sin(angle);return (mulib_geom2_point){center.x+c*x-s*y,center.y+s*x+c*y};}
int mulib_canvas_gizmo_scale(mulib_geom2_point p,mulib_geom2_point anchor,mulib_geom2_point scale,mulib_geom2_point *out){if(!out||!mulib_geom2_point_valid(p)||!mulib_geom2_point_valid(anchor)||!mulib_geom2_point_valid(scale)||scale.x==0||scale.y==0)return 0;*out=(mulib_geom2_point){anchor.x+(p.x-anchor.x)*scale.x,anchor.y+(p.y-anchor.y)*scale.y};return mulib_geom2_point_valid(*out);}
#ifdef __cplusplus
}
#endif
#endif
