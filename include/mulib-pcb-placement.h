/* MuLIB / Amb3r: pcb-placement. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_PLACEMENT in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_PLACEMENT_H
#define MULIB_PCB_PLACEMENT_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_pcb_placement_shelf(const ma_point *sizes,size_t n,double width,double gap,ma_point *positions,double *height);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_PLACEMENT) && !defined(MULIB_PCB_PLACEMENT_H_IMPLEMENTED)
#define MULIB_PCB_PLACEMENT_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_placement_shelf(const ma_point *sizes,size_t n,double width,double gap,ma_point *positions,double *height){if((!sizes&&n)||(!positions&&n)||!height||width<=0||gap<0)return 0;double x=0,y=0,row=0;for(size_t i=0;i<n;i++){if(!ma_point_valid(sizes[i])||sizes[i].x<=0||sizes[i].y<=0||sizes[i].x>width)return 0;if(x+sizes[i].x>width){x=0;y+=row+gap;row=0;}positions[i]=(ma_point){x,y};x+=sizes[i].x+gap;row=MA_MAX(row,sizes[i].y);}*height=y+row;return 1;}
#ifdef __cplusplus
}
#endif
#endif
