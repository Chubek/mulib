/* MuLIB / Amb3r: pcb-pour. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_POUR in exactly one implementation translation unit.
 * Profile: Grid-sampled polygon copper fill with circular keepouts, not exact polygon clipping/thermals.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_POUR_H
#define MULIB_PCB_POUR_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {ma_point center;double radius;} ma_circle;
int mulib_pcb_pour(const ma_point *poly,size_t n,ma_point origin,double step,size_t w,size_t h,const ma_circle *keep,size_t nk,unsigned char *mask);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_POUR) && !defined(MULIB_PCB_POUR_H_IMPLEMENTED)
#define MULIB_PCB_POUR_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_pour(const ma_point *poly,size_t n,ma_point origin,double step,size_t w,size_t h,const ma_circle *keep,size_t nk,unsigned char *mask){size_t count;if(!poly||n<3||!mask||!ma_mul(w,h,&count)||!isfinite(step)||step<=0||(!keep&&nk))return 0;for(size_t i=0;i<nk;i++)if(!ma_point_valid(keep[i].center)||!isfinite(keep[i].radius)||keep[i].radius<0)return 0;for(size_t y=0;y<h;y++)for(size_t x=0;x<w;x++){ma_point q={origin.x+(x+.5)*step,origin.y+(y+.5)*step};int ok=ma_poly_inside(poly,n,q);for(size_t i=0;i<nk&&ok;i++)if(ma_dist(q,keep[i].center)<=keep[i].radius)ok=0;mask[y*w+x]=(unsigned char)ok;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
