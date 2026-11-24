/** @file mulib-anim-bake.h
 * Bake a scalar curve into uniform integer-tick samples.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Scalar samples; output preserved on error.
 */
#ifndef MULIB_ANIM_BAKE_H
#define MULIB_ANIM_BAKE_H
#include "mulib-mesh-core.h"
#include "mulib-anim-channel.h"
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
/* out[n] doubles, disjoint from ch and its keys. step is positive ticks.
 * First pass validates every sample/time without writing out. */
static inline bool mulib_anim_bake(const mulib_anim_channel *ch,int64_t start,int64_t step,double *out,size_t n){
 if(!ch||(!out&&n)||n>SIZE_MAX/sizeof *out||ch->count>SIZE_MAX/sizeof *ch->keys||step<=0||!mulib_mesh_disjoint(ch,sizeof *ch,out,n*sizeof *out)||!mulib_mesh_disjoint(ch->keys,ch->count*sizeof *ch->keys,out,n*sizeof *out))return false;
 int64_t t=start;for(size_t i=0;i<n;i++){double value;if(!mulib_anim_curve_eval(ch->keys,ch->count,t,ch->mode,&value))return false;if(i+1<n&&!mulib_media_i64_add(t,step,&t))return false;}
 t=start;for(size_t i=0;i<n;i++){if(!mulib_anim_curve_eval(ch->keys,ch->count,t,ch->mode,&out[i]))return false;if(i+1<n&&!mulib_media_i64_add(t,step,&t))return false;}
 return true;
}
#ifdef __cplusplus
}
#endif
#endif
