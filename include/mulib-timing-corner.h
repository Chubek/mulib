/* MuLIB / Amb3r: timing-corner. C11 single-header kernel.
 * Define MULIB_IMPL_TIMING_CORNER in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_TIMING_CORNER_H
#define MULIB_TIMING_CORNER_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_timing_corner_scale(const ma_edge *edges,size_t n,double factor,ma_edge *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TIMING_CORNER) && !defined(MULIB_TIMING_CORNER_H_IMPLEMENTED)
#define MULIB_TIMING_CORNER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_timing_corner_scale(const ma_edge *edges,size_t n,double factor,ma_edge *out){if((!edges&&n)||(!out&&n)||factor<=0||!isfinite(factor))return 0;for(size_t i=0;i<n;i++){if(edges[i].weight<0||!isfinite(edges[i].weight*factor))return 0;out[i]=edges[i];out[i].weight*=factor;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
