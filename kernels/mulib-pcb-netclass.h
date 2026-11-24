/* MuLIB: pcb-netclass. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_NETCLASS in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_NETCLASS_H
#define MULIB_PCB_NETCLASS_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-storage-core.h"
#include "mulib-geom2-core.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {double clearance,min_width,max_width,min_drill;} mulib_pcb_netclass;
int mulib_pcb_netclass_combine(const mulib_pcb_netclass *a,const mulib_pcb_netclass *b,mulib_pcb_netclass *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_NETCLASS) && !defined(MULIB_PCB_NETCLASS_H_IMPLEMENTED)
#define MULIB_PCB_NETCLASS_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_netclass_combine(const mulib_pcb_netclass *a,const mulib_pcb_netclass *b,mulib_pcb_netclass *out){if(!a||!b||!out)return 0;mulib_pcb_netclass r={MULIB_STORAGE_MAX(a->clearance,b->clearance),MULIB_STORAGE_MAX(a->min_width,b->min_width),MULIB_STORAGE_MIN(a->max_width,b->max_width),MULIB_STORAGE_MAX(a->min_drill,b->min_drill)};if(!isfinite(r.clearance)||!isfinite(r.min_width)||!isfinite(r.max_width)||!isfinite(r.min_drill)||r.clearance<0||r.min_width<=0||r.max_width<r.min_width||r.min_drill<=0)return 0;*out=r;return 1;}
#ifdef __cplusplus
}
#endif
#endif
