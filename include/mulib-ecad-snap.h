/* MuLIB / Amb3r: ecad-snap. C11 single-header kernel.
 * Define MULIB_IMPL_ECAD_SNAP in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_ECAD_SNAP_H
#define MULIB_ECAD_SNAP_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_ecad_snap(ma_point input,double spacing,const ma_point *targets,size_t n,double radius,ma_point *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ECAD_SNAP) && !defined(MULIB_ECAD_SNAP_H_IMPLEMENTED)
#define MULIB_ECAD_SNAP_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ecad_snap(ma_point input,double spacing,const ma_point *targets,size_t n,double radius,ma_point *out){if(!out||!ma_point_valid(input)||!isfinite(spacing)||spacing<=0||radius<0||(!targets&&n))return 0;*out=(ma_point){round(input.x/spacing)*spacing,round(input.y/spacing)*spacing};double best=radius;for(size_t i=0;i<n;i++){double d=ma_dist(input,targets[i]);if(d<=best){*out=targets[i];best=d;}}return ma_point_valid(*out);}
#ifdef __cplusplus
}
#endif
#endif
