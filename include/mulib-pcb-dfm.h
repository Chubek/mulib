/* MuLIB / Amb3r: pcb-dfm. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_DFM in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_DFM_H
#define MULIB_PCB_DFM_H
#include "mulib-amb3r-base.h"
#include "mulib-pcb-via.h"
#ifdef __cplusplus
extern "C" {
#endif

unsigned mulib_pcb_dfm_via(const ma_pcb_via *v,double min_drill,double min_ring,double thickness,double max_aspect);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_DFM) && !defined(MULIB_PCB_DFM_H_IMPLEMENTED)
#define MULIB_PCB_DFM_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
unsigned mulib_pcb_dfm_via(const ma_pcb_via *v,double min_drill,double min_ring,double thickness,double max_aspect){if(!v||min_drill<=0||min_ring<0||thickness<=0||max_aspect<=0||!isfinite(v->drill)||!isfinite(v->diameter))return UINT_MAX;unsigned flags=0;if(v->drill<min_drill)flags|=1;if((v->diameter-v->drill)/2<min_ring)flags|=2;if(v->drill<=0||thickness/v->drill>max_aspect)flags|=4;return flags;}
#ifdef __cplusplus
}
#endif
#endif
