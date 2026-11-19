/* MuLIB / Amb3r: pcb-pad. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_PAD in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_PAD_H
#define MULIB_PCB_PAD_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t id,net;ma_point center;double width,height,angle;unsigned shape,layer;} ma_pcb_pad;
int mulib_pcb_pad_contains(const ma_pcb_pad *p,ma_point q);
ma_box mulib_pcb_pad_bounds(const ma_pcb_pad *p);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_PAD) && !defined(MULIB_PCB_PAD_H_IMPLEMENTED)
#define MULIB_PCB_PAD_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_pad_contains(const ma_pcb_pad *p,ma_point q){if(!p||p->width<=0||p->height<=0||!isfinite(p->angle)||!ma_point_valid(q)||!ma_point_valid(p->center))return 0;double x=q.x-p->center.x,y=q.y-p->center.y,c=cos(p->angle),s=sin(p->angle),u=c*x+s*y,v=-s*x+c*y;if(p->shape==0)return fabs(u)<=p->width/2&&fabs(v)<=p->height/2;if(p->shape==1)return hypot(2*u/p->width,2*v/p->height)<=1;return 0;}
ma_box mulib_pcb_pad_bounds(const ma_pcb_pad *p){double c=fabs(cos(p->angle)),s=fabs(sin(p->angle)),x=(c*p->width+s*p->height)/2,y=(s*p->width+c*p->height)/2;ma_box b={{p->center.x-x,p->center.y-y},{p->center.x+x,p->center.y+y}};return b;}
#ifdef __cplusplus
}
#endif
#endif
