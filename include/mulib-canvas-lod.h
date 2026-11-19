/* MuLIB / Amb3r: canvas-lod. C11 single-header kernel.
 * Define MULIB_IMPL_CANVAS_LOD in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_CANVAS_LOD_H
#define MULIB_CANVAS_LOD_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

unsigned mulib_canvas_lod(double world_size,double zoom,const double *pixel_thresholds,size_t n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CANVAS_LOD) && !defined(MULIB_CANVAS_LOD_H_IMPLEMENTED)
#define MULIB_CANVAS_LOD_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
unsigned mulib_canvas_lod(double world_size,double zoom,const double *pixel_thresholds,size_t n){if(world_size<0||zoom<=0||!isfinite(world_size*zoom)||(!pixel_thresholds&&n)||n>UINT_MAX)return UINT_MAX;double p=world_size*zoom;for(size_t i=0;i<n;i++)if(!isfinite(pixel_thresholds[i])||pixel_thresholds[i]<0||(i&&pixel_thresholds[i]<=pixel_thresholds[i-1]))return UINT_MAX;size_t i=0;while(i<n&&p>=pixel_thresholds[i])i++;return (unsigned)i;}
#ifdef __cplusplus
}
#endif
#endif
