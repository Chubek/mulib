/** @file mulib-audio-filter.h
 * Stateful DF-II transposed biquad, lowpass design and causal FIR.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: FIR zero initial history; one biquad state per channel.
 */
#ifndef MULIB_AUDIO_FILTER_H
#define MULIB_AUDIO_FILTER_H
#include "mulib-audio-core.h"
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
typedef struct {double b0,b1,b2,a1,a2,z1,z2;} mulib_audio_filter_biquad;
static inline bool mulib_audio_filter_valid(const mulib_audio_filter_biquad *s){return s&&isfinite(s->b0)&&isfinite(s->b1)&&isfinite(s->b2)&&isfinite(s->a1)&&isfinite(s->a2)&&isfinite(s->z1)&&isfinite(s->z2);}
/* Unchecked realtime primitive: initialized writable s, finite x, and stable
 * finite coefficients/state are required. Arithmetic overflow may poison state. */
static inline float mulib_audio_filter_tick(mulib_audio_filter_biquad *s,float x){double y=s->b0*x+s->z1;s->z1=s->b1*x-s->a1*y+s->z2;s->z2=s->b2*x-s->a2*y;return mulib_audio_core_float(y);}
static inline bool mulib_audio_filter_lowpass(mulib_audio_filter_biquad *s,double rate,double hz,double q){if(!s||!isfinite(rate)||!isfinite(hz)||!isfinite(q)||rate<=0||hz<=0||hz>=rate/2||q<=0)return false;double w=6.283185307179586*(hz/rate),c=cos(w),a=(sin(w)/q)*.5,d=1+a;mulib_audio_filter_biquad z={((1-c)/d)*.5,(1-c)/d,((1-c)/d)*.5,-2*c/d,(1-a)/d,0,0};if(!mulib_audio_filter_valid(&z))return false;*s=z;return true;}
/* out/in have n floats, k has taps floats. Output must be disjoint from
 * both read spans; all inputs finite. Zero initial history; no allocation. */
static inline bool mulib_audio_filter_fir(float *out,const float *in,size_t n,const float *k,size_t taps){
 size_t bytes,kb;
 if(!out||!in||!k||!taps||!mulib_audio_core_bytes(n,1,&bytes)||!mulib_audio_core_bytes(taps,1,&kb)||
    !mulib_audio_core_disjoint(out,bytes,in,bytes)||!mulib_audio_core_disjoint(out,bytes,k,kb)||
    !mulib_audio_core_finite(in,n)||!mulib_audio_core_finite(k,taps))return false;
 for(size_t i=0;i<n;i++){double y=0;for(size_t j=0;j<taps&&j<=i;j++)y+=(double)k[j]*in[i-j];out[i]=mulib_audio_core_float(y);}return true;
}
#ifdef __cplusplus
}
#endif
#endif
