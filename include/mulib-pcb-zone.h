/* MuLIB / Amb3r: pcb-zone. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_ZONE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_ZONE_H
#define MULIB_PCB_ZONE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

double mulib_pcb_zone_area(const ma_point *p,size_t n);
int mulib_pcb_zone_contains(const ma_point *p,size_t n,ma_point q);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_ZONE) && !defined(MULIB_PCB_ZONE_H_IMPLEMENTED)
#define MULIB_PCB_ZONE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
double mulib_pcb_zone_area(const ma_point *p,size_t n){if(!p||n<3)return NAN;double a=0;for(size_t i=0,j=n-1;i<n;j=i++){if(!ma_point_valid(p[i]))return NAN;a+=p[j].x*p[i].y-p[i].x*p[j].y;}return fabs(a)*.5;}
int mulib_pcb_zone_contains(const ma_point *p,size_t n,ma_point q){return ma_poly_inside(p,n,q);}
#ifdef __cplusplus
}
#endif
#endif
