/** @file mulib-audio-eq.h
 * RBJ peaking EQ design and cascaded mono processing.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_AUDIO_EQ_H
#define MULIB_AUDIO_EQ_H
#include "mulib-media-core.h"
#include "mulib-audio-filter.h"
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
static inline bool mulib_audio_eq_peak(mulib_audio_filter_biquad *s,double rate,double hz,double q,double db){if(!s||!isfinite(rate)||!isfinite(hz)||!isfinite(q)||!isfinite(db)||rate<=0||hz<=0||hz>=rate/2||q<=0||fabs(db)>60)return false;double w=6.283185307179586*(hz/rate),A=pow(10,db/40),a=(sin(w)/q)*.5,c=cos(w),d=1+a/A;mulib_audio_filter_biquad z={(1+a*A)/d,-2*c/d,(1-a*A)/d,-2*c/d,(1-a/A)/d,0,0};if(!mulib_audio_filter_valid(&z))return false;*s=z;return true;}
/* p has n floats, bands has count initialized states, with disjoint storage.
 * Invalid input is rejected before mutation. Later arithmetic overflow follows
 * tick semantics and can leave partial output/state (no rollback). */
static inline bool mulib_audio_eq_process(float *p,size_t n,mulib_audio_filter_biquad *bands,size_t count){
 size_t pb,bb;
 if(!p||(!bands&&count)||!mulib_audio_core_bytes(n,1,&pb)||!mulib_media_size_mul(count,sizeof *bands,&bb)||
    !mulib_audio_core_disjoint(p,pb,bands,bb)||!mulib_audio_core_finite(p,n))return false;
 for(size_t j=0;j<count;j++)if(!mulib_audio_filter_valid(bands+j))return false;
 for(size_t i=0;i<n;i++)for(size_t j=0;j<count;j++)p[i]=mulib_audio_filter_tick(bands+j,p[i]);
 return true;
}
#ifdef __cplusplus
}
#endif
#endif
