/* MuLIB / Amb3r: canvas-gizmo. C11 single-header kernel.
 * Define MULIB_IMPL_CANVAS_GIZMO in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_CANVAS_GIZMO_H
#define MULIB_CANVAS_GIZMO_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

ma_point mulib_canvas_gizmo_rotate(ma_point p,ma_point center,double angle);
int mulib_canvas_gizmo_scale(ma_point p,ma_point anchor,ma_point scale,ma_point *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CANVAS_GIZMO) && !defined(MULIB_CANVAS_GIZMO_H_IMPLEMENTED)
#define MULIB_CANVAS_GIZMO_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
ma_point mulib_canvas_gizmo_rotate(ma_point p,ma_point center,double angle){double x=p.x-center.x,y=p.y-center.y,c=cos(angle),s=sin(angle);return (ma_point){center.x+c*x-s*y,center.y+s*x+c*y};}
int mulib_canvas_gizmo_scale(ma_point p,ma_point anchor,ma_point scale,ma_point *out){if(!out||!ma_point_valid(p)||!ma_point_valid(anchor)||!ma_point_valid(scale)||scale.x==0||scale.y==0)return 0;*out=(ma_point){anchor.x+(p.x-anchor.x)*scale.x,anchor.y+(p.y-anchor.y)*scale.y};return ma_point_valid(*out);}
#ifdef __cplusplus
}
#endif
#endif
