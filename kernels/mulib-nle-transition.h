/** @file mulib-nle-transition.h
 * Linear and smooth crossfade weights.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_NLE_TRANSITION_H
#define MULIB_NLE_TRANSITION_H
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
static inline bool mulib_nle_transition_weights(int64_t start,int64_t length,int64_t t,bool smooth,float *a,float *b){int64_t end;if(!a||!b||length<=0||!mulib_media_i64_add(start,length,&end))return false;double x=t<=start?0:t>=end?1:(double)(t-start)/length;if(smooth)x=x*x*(3-2*x);*b=(float)x;*a=1-*b;return true;}
#ifdef __cplusplus
}
#endif
#endif
