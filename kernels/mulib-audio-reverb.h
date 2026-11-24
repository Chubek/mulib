/** @file mulib-audio-reverb.h
 * Parallel feedback comb reverb.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Set comb wet=1; no diffusion allpass or damping filter.
 */
#ifndef MULIB_AUDIO_REVERB_H
#define MULIB_AUDIO_REVERB_H
#include "mulib-media-core.h"
#include "mulib-audio-delay.h"
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
/* Parallel comb delays, each using independent caller-owned delay rings. */
/* p has n finite floats; comb has count initialized independent rings with
 * wet==1. All writable spans must be pairwise disjoint. Invalid descriptors/
 * input are rejected before writes. Caller guarantees finite ring contents;
 * arithmetic overflow can poison state and is not transactionally rolled back. */
static inline bool mulib_audio_reverb_process(float *p,size_t n,mulib_audio_delay *comb,size_t count,float wet){
 size_t pb,cb;
 if(!p||!comb||!count||!isfinite(wet)||wet<0||wet>1||!mulib_audio_core_bytes(n,1,&pb)||
    !mulib_media_size_mul(count,sizeof *comb,&cb)||!mulib_audio_core_disjoint(p,pb,comb,cb))return false;
 for(size_t j=0;j<count;j++){
  size_t rb;
  if(!comb[j].buffer||!comb[j].length||comb[j].cursor>=comb[j].length||
     !isfinite(comb[j].feedback)||fabsf(comb[j].feedback)>=1||comb[j].wet!=1||
     !mulib_audio_core_bytes(comb[j].length,1,&rb)||!mulib_audio_core_disjoint(p,pb,comb[j].buffer,rb)||
     !mulib_audio_core_disjoint(comb,cb,comb[j].buffer,rb))return false;
  for(size_t k=0;k<j;k++)if(!mulib_audio_core_disjoint(comb[k].buffer,comb[k].length*sizeof(float),comb[j].buffer,rb))return false;
 }
 if(!mulib_audio_core_finite(p,n))return false;
 for(size_t i=0;i<n;i++){float x=p[i];double a=0;for(size_t j=0;j<count;j++)a+=mulib_audio_delay_tick(comb+j,x);p[i]=mulib_audio_core_float((double)x*(1-wet)+(a/count)*wet);}return true;
}
#ifdef __cplusplus
}
#endif
#endif
