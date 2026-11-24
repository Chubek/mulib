/** @file mulib-nle-edl.h
 * Write lossless neutral tab-separated edit list.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Not CMX3600; implement SMPTE/drop-frame interchange separately.
 */
#ifndef MULIB_NLE_EDL_H
#define MULIB_NLE_EDL_H
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
/* Neutral tab-separated interchange: id asset track start duration source. */
static inline int mulib_nle_edl_write(FILE *f,const mulib_nle_timeline *t){if(!f||!mulib_nle_timeline_valid(t))return -1;for(size_t i=0;i<t->count;i++){mulib_nle_item c=t->items[i];if(!mulib_nle_item_valid(c)||fprintf(f,"%llu\t%llu\t%llu\t%lld\t%lld\t%lld\n",(unsigned long long)c.id,(unsigned long long)c.asset,(unsigned long long)c.track,(long long)c.start,(long long)c.length,(long long)c.source)<0)return -1;}return 0;}
#ifdef __cplusplus
}
#endif
#endif
