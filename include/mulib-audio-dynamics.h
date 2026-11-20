/** @file mulib-audio-dynamics.h
 * Peak envelope compressor and hard limiter.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No lookahead, RMS detection, gate or linked channels.
 */
#ifndef MULIB_AUDIO_DYNAMICS_H
#define MULIB_AUDIO_DYNAMICS_H
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
typedef struct {float envelope;float threshold,ratio,attack_coeff,release_coeff;} mulib_audio_dynamics;
/* rate is Hz; attack/release seconds. Reject times that round the f32
 * smoothing coefficient to 1 (a permanently frozen envelope). */
static inline bool mulib_audio_dynamics_init(mulib_audio_dynamics *d,float threshold,float ratio,double rate,double attack,double release){
 if(!d||!isfinite(threshold)||threshold<=0||!isfinite(ratio)||ratio<1||!isfinite(rate)||rate<=0||!isfinite(attack)||attack<=0||!isfinite(release)||release<=0)return false;
 float ac=(float)exp((-1/rate)/attack),rc=(float)exp((-1/rate)/release);
 if(!isfinite(ac)||!isfinite(rc)||ac<0||ac>=1||rc<0||rc>=1)return false;
 mulib_audio_dynamics z={0,threshold,ratio,ac,rc};*d=z;return true;
}
/* Unchecked realtime primitive: initialized d, finite x, exclusive access. */
static inline float mulib_audio_dynamics_tick(mulib_audio_dynamics *d,float x){double a=fabs((double)x),c=a>d->envelope?d->attack_coeff:d->release_coeff;d->envelope=mulib_audio_core_float(c*d->envelope+(1-c)*a);double g=d->envelope>d->threshold?pow((double)d->threshold/d->envelope,1-1/(double)d->ratio):1;return mulib_audio_core_float(x*g);}
static inline float mulib_audio_dynamics_limit(float x,float ceiling){return ceiling>0?mulib_media_clampf(x,-ceiling,ceiling):0;}
#ifdef __cplusplus
}
#endif
#endif
