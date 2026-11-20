/** @file mulib-nle-snap.h
 * Nearest clip boundary snapping with deterministic ties.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_NLE_SNAP_H
#define MULIB_NLE_SNAP_H
#include "mulib-nle-timeline.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Exact unsigned distances avoid overflow and floating-point tie changes at
 * large tick values. Output is unchanged if no boundary is within tolerance. */
static inline bool mulib_nle_snap_nearest(const mulib_nle_timeline *t,uint64_t ignore,int64_t tick,int64_t tolerance,int64_t *out){if(!mulib_nle_timeline_valid(t)||!out||tolerance<0)return false;uint64_t best=0;int64_t selected=0;bool found=false;for(size_t i=0;i<t->count;i++){mulib_nle_item c=t->items[i];if(c.id==ignore)continue;int64_t k[2]={c.start,c.start+c.length};for(int j=0;j<2;j++){uint64_t d=k[j]>=tick?(uint64_t)k[j]-(uint64_t)tick:(uint64_t)tick-(uint64_t)k[j];if(d<=(uint64_t)tolerance&&(!found||d<best||(d==best&&k[j]<selected))){best=d;selected=k[j];found=true;}}}if(found)*out=selected;return found;}
#ifdef __cplusplus
}
#endif
#endif
