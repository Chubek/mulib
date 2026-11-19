/* MuLIB / Amb3r: pcb-netclass. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_NETCLASS in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_NETCLASS_H
#define MULIB_PCB_NETCLASS_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {double clearance,min_width,max_width,min_drill;} ma_netclass;
int mulib_pcb_netclass_combine(const ma_netclass *a,const ma_netclass *b,ma_netclass *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_NETCLASS) && !defined(MULIB_PCB_NETCLASS_H_IMPLEMENTED)
#define MULIB_PCB_NETCLASS_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_netclass_combine(const ma_netclass *a,const ma_netclass *b,ma_netclass *out){if(!a||!b||!out)return 0;ma_netclass r={MA_MAX(a->clearance,b->clearance),MA_MAX(a->min_width,b->min_width),MA_MIN(a->max_width,b->max_width),MA_MAX(a->min_drill,b->min_drill)};if(!isfinite(r.clearance)||!isfinite(r.min_width)||!isfinite(r.max_width)||!isfinite(r.min_drill)||r.clearance<0||r.min_width<=0||r.max_width<r.min_width||r.min_drill<=0)return 0;*out=r;return 1;}
#ifdef __cplusplus
}
#endif
#endif
