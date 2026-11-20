/** @file mulib-nle-retime.h
 * Piecewise-linear source-time mapping, including reverse and hold.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_NLE_RETIME_H
#define MULIB_NLE_RETIME_H
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
typedef struct {int64_t tick;double source;} mulib_nle_retime_key;
static inline bool mulib_nle_retime_eval(const mulib_nle_retime_key *k,size_t n,int64_t tick,double *out){if(!k||!n||!out)return false;for(size_t i=0;i<n;i++)if(!isfinite(k[i].source)||(i&&k[i].tick<=k[i-1].tick))return false;if(tick<=k[0].tick){*out=k[0].source;return true;}for(size_t i=1;i<n;i++)if(tick<=k[i].tick){double u=(double)(((long double)tick-k[i-1].tick)/((long double)k[i].tick-k[i-1].tick));*out=(1-u)*k[i-1].source+u*k[i].source;return true;}*out=k[n-1].source;return true;}
#ifdef __cplusplus
}
#endif
#endif
