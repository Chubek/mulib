/* MuLIB / Amb3r: pcb-thermal-relief. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_THERMAL_RELIEF in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_THERMAL_RELIEF_H
#define MULIB_PCB_THERMAL_RELIEF_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_pcb_thermal_relief(ma_point center,double inner,double outer,unsigned spokes,double angle,ma_point *segments,size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_THERMAL_RELIEF) && !defined(MULIB_PCB_THERMAL_RELIEF_H_IMPLEMENTED)
#define MULIB_PCB_THERMAL_RELIEF_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_thermal_relief(ma_point center,double inner,double outer,unsigned spokes,double angle,ma_point *segments,size_t capacity){if(!segments||!spokes||spokes>capacity/2||inner<0||outer<=inner||!isfinite(angle)||!ma_point_valid(center))return 0;for(unsigned i=0;i<spokes;i++){double t=angle+2*MA_PI*i/spokes;segments[2*i]=(ma_point){center.x+inner*cos(t),center.y+inner*sin(t)};segments[2*i+1]=(ma_point){center.x+outer*cos(t),center.y+outer*sin(t)};}return 1;}
#ifdef __cplusplus
}
#endif
#endif
