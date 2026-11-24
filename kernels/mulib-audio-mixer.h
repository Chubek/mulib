/** @file mulib-audio-mixer.h
 * Allocation-free multitrack mono/stereo mixing with pan and gain.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Inputs must not overlap output; no limiter or solo handling.
 */
#ifndef MULIB_AUDIO_MIXER_H
#define MULIB_AUDIO_MIXER_H
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
typedef struct {const float *samples;size_t frames,channels;float gain,pan;bool mute;} mulib_audio_mixer_track;
/* out has 2*frames floats; t has count descriptors; each unmuted samples span
 * has track.frames*channels floats. Output is disjoint from descriptors and all
 * input spans. Only min(frames,track.frames) samples are read/finite-validated.
 * Equal-power mono pan, stereo balance; overflow can yield Inf/NaN, no limiter.
 * Validation failure leaves output unchanged. */
static inline bool mulib_audio_mixer(float *out,size_t frames,const mulib_audio_mixer_track *t,size_t count){
 size_t ob,tb;
 if(!out||(!t&&count)||!mulib_audio_core_bytes(frames,2,&ob)||!mulib_media_size_mul(count,sizeof *t,&tb)||
    !mulib_audio_core_disjoint(out,ob,t,tb))return false;
 for(size_t i=0;i<count;i++)if(!t[i].mute){
  size_t ib,n=frames<t[i].frames?frames:t[i].frames;
  if((t[i].frames&&!t[i].samples)||(t[i].channels!=1&&t[i].channels!=2)||
     !mulib_audio_core_bytes(t[i].frames,t[i].channels,&ib)||!mulib_audio_core_disjoint(out,ob,t[i].samples,ib)||
     !isfinite(t[i].gain)||!isfinite(t[i].pan)||fabsf(t[i].pan)>1||
     !mulib_audio_core_finite(t[i].samples,n*t[i].channels))return false;
 }
 memset(out,0,ob);
 for(size_t i=0;i<count;i++){if(t[i].mute)continue;double a=t[i].gain,b=a;
  if(t[i].channels==1){double p=((double)t[i].pan+1)*.7853981633974483;a*=cos(p);b*=sin(p);}
  else{a*=1-fmax(0,t[i].pan);b*=1+fmin(0,t[i].pan);}
  size_t n=frames<t[i].frames?frames:t[i].frames;
  for(size_t j=0;j<n;j++){
   out[2*j]=mulib_audio_core_float((double)out[2*j]+a*t[i].samples[j*t[i].channels]);
   out[2*j+1]=mulib_audio_core_float((double)out[2*j+1]+b*t[i].samples[j*t[i].channels+t[i].channels-1]);
  }
 }return true;
}
#ifdef __cplusplus
}
#endif
#endif
