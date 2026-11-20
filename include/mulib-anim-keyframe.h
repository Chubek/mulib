/** @file mulib-anim-keyframe.h
 * Sorted integer-time scalar keyframes and insertion.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_ANIM_KEYFRAME_H
#define MULIB_ANIM_KEYFRAME_H
#include "mulib-mesh-core.h"
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
typedef struct {int64_t tick;double value,in_tangent,out_tangent;} mulib_anim_key;
static inline bool mulib_anim_keyframe_valid(const mulib_anim_key *k,size_t n){if(!k||!n||n>SIZE_MAX/sizeof *k)return false;for(size_t i=0;i<n;i++)if(!isfinite(k[i].value)||!isfinite(k[i].in_tangent)||!isfinite(k[i].out_tangent)||(i&&k[i].tick<=k[i-1].tick))return false;return true;}
static inline size_t mulib_anim_keyframe_floor(const mulib_anim_key *k,size_t n,int64_t t){if(!k||!n)return SIZE_MAX;size_t lo=0,hi=n;while(lo<hi){size_t m=lo+(hi-lo)/2;if(k[m].tick<=t)lo=m+1;else hi=m;}return lo?lo-1:SIZE_MAX;}
static inline bool mulib_anim_keyframe_insert(mulib_anim_key *k,size_t *n,size_t cap,mulib_anim_key v){if(!k||!n||cap>SIZE_MAX/sizeof *k||!mulib_mesh_disjoint(k,cap*sizeof *k,n,sizeof *n)||*n>cap||!isfinite(v.value)||!isfinite(v.in_tangent)||!isfinite(v.out_tangent)||(*n&&!mulib_anim_keyframe_valid(k,*n)))return false;size_t p=mulib_anim_keyframe_floor(k,*n,v.tick);if(p!=SIZE_MAX&&k[p].tick==v.tick){k[p]=v;return true;}p=p==SIZE_MAX?0:p+1;if(*n==cap)return false;memmove(k+p+1,k+p,(*n-p)*sizeof *k);k[p]=v;(*n)++;return true;}
#ifdef __cplusplus
}
#endif
#endif
