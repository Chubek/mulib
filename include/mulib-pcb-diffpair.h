/* MuLIB / Amb3r: pcb-diffpair. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_DIFFPAIR in exactly one implementation translation unit.
 * Profile: Polyline offset reference profile; does not enforce corner radius or clearance.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_DIFFPAIR_H
#define MULIB_PCB_DIFFPAIR_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_pcb_diffpair_offset(const ma_point *center,size_t n,double separation,ma_point *a,ma_point *b);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_DIFFPAIR) && !defined(MULIB_PCB_DIFFPAIR_H_IMPLEMENTED)
#define MULIB_PCB_DIFFPAIR_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_diffpair_offset(const ma_point *center,size_t n,double separation,ma_point *a,ma_point *b){if(!center||n<2||!a||!b||!isfinite(separation)||separation<=0)return 0;for(size_t i=0;i<n;i++){ma_point l=center[i?i-1:i],r=center[i+1<n?i+1:i];double dx=r.x-l.x,dy=r.y-l.y,d=hypot(dx,dy);if(!d||!isfinite(d))return 0;double x=-dy*separation/(2*d),y=dx*separation/(2*d);a[i]=(ma_point){center[i].x+x,center[i].y+y};b[i]=(ma_point){center[i].x-x,center[i].y-y};}return 1;}
#ifdef __cplusplus
}
#endif
#endif
