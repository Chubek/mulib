/** @file mulib-anim-channel.h
 * Evaluate multiple scalar animation property channels.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Scalar channels only; validates all results before output mutation.
 */
#ifndef MULIB_ANIM_CHANNEL_H
#define MULIB_ANIM_CHANNEL_H
#include "mulib-mesh-core.h"
#include "mulib-anim-curve.h"
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
typedef struct {const mulib_anim_key *keys;size_t count;mulib_anim_curve_mode mode;} mulib_anim_channel;
/* ch[n], out[n] doubles, with out disjoint from channel/key storage. Two
 * deterministic passes preserve all outputs on invalid or overflowing curves. */
static inline bool mulib_anim_channel_eval(const mulib_anim_channel *ch,size_t n,int64_t tick,double *out){
 if(((!ch||!out)&&n)||n>SIZE_MAX/sizeof *ch||n>SIZE_MAX/sizeof *out||!mulib_mesh_disjoint(ch,n*sizeof *ch,out,n*sizeof *out))return false;
 for(size_t i=0;i<n;i++){double value;if(ch[i].count>SIZE_MAX/sizeof *ch[i].keys||!mulib_mesh_disjoint(ch[i].keys,ch[i].count*sizeof *ch[i].keys,out,n*sizeof *out)||!mulib_anim_curve_eval(ch[i].keys,ch[i].count,tick,ch[i].mode,&value))return false;}
 for(size_t i=0;i<n;i++)if(!mulib_anim_curve_eval(ch[i].keys,ch[i].count,tick,ch[i].mode,&out[i]))return false;
 return true;
}
#ifdef __cplusplus
}
#endif
#endif
