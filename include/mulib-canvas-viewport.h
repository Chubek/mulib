/* MuLIB / Amb3r: canvas-viewport. C11 single-header kernel.
 * Define MULIB_IMPL_CANVAS_VIEWPORT in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_CANVAS_VIEWPORT_H
#define MULIB_CANVAS_VIEWPORT_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_canvas_viewport_clip(ma_box box,ma_point *a,ma_point *b);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CANVAS_VIEWPORT) && !defined(MULIB_CANVAS_VIEWPORT_H_IMPLEMENTED)
#define MULIB_CANVAS_VIEWPORT_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_canvas_viewport_clip(ma_box box,ma_point *a,ma_point *b){if(!a||!b||!ma_box_valid(box)||!ma_point_valid(*a)||!ma_point_valid(*b))return 0;double dx=b->x-a->x,dy=b->y-a->y,p[4]={-dx,dx,-dy,dy},q[4]={a->x-box.lo.x,box.hi.x-a->x,a->y-box.lo.y,box.hi.y-a->y},lo=0,hi=1;for(unsigned i=0;i<4;i++){if(p[i]==0){if(q[i]<0)return 0;}else{double t=q[i]/p[i];if(p[i]<0)lo=MA_MAX(lo,t);else hi=MA_MIN(hi,t);if(lo>hi)return 0;}}ma_point start=*a;*a=(ma_point){start.x+lo*dx,start.y+lo*dy};*b=(ma_point){start.x+hi*dx,start.y+hi*dy};return 1;}
#ifdef __cplusplus
}
#endif
#endif
