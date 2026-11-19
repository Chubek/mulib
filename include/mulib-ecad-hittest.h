/* MuLIB / Amb3r: ecad-hittest. C11 single-header kernel.
 * Define MULIB_IMPL_ECAD_HITTEST in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_ECAD_HITTEST_H
#define MULIB_ECAD_HITTEST_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

size_t mulib_ecad_hittest(const ma_box *objects,size_t count,ma_point point,double tolerance);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ECAD_HITTEST) && !defined(MULIB_ECAD_HITTEST_H_IMPLEMENTED)
#define MULIB_ECAD_HITTEST_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_ecad_hittest(const ma_box *objects,size_t count,ma_point point,double tolerance){if((!objects&&count)||!ma_point_valid(point)||!isfinite(tolerance)||tolerance<0)return MA_NONE;for(size_t i=count;i-->0;)if(ma_box_valid(objects[i])&&point.x>=objects[i].lo.x-tolerance&&point.x<=objects[i].hi.x+tolerance&&point.y>=objects[i].lo.y-tolerance&&point.y<=objects[i].hi.y+tolerance)return i;return MA_NONE;}
#ifdef __cplusplus
}
#endif
#endif
