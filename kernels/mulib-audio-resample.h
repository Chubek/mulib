/** @file mulib-audio-resample.h
 * Interleaved linear sample-rate conversion; ratio=input/output rate.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No anti-alias filter, streaming phase history or sinc quality.
 */
#ifndef MULIB_AUDIO_RESAMPLE_H
#define MULIB_AUDIO_RESAMPLE_H
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
/* out/in hold out_frames*ch / in_frames*ch floats, and must be disjoint.
 * All input samples must be finite. Coordinates past the input clamp to its
 * final frame (including overflowed phase+i*ratio). Validation is atomic. */
static inline bool mulib_audio_resample_linear(float *out,size_t out_frames,const float *in,size_t in_frames,size_t ch,double ratio,double phase){
 size_t ob,ib;
 if(!out||!in||!in_frames||!mulib_audio_core_bytes(in_frames,ch,&ib)||!mulib_audio_core_bytes(out_frames,ch,&ob)||
    !mulib_audio_core_disjoint(out,ob,in,ib)||!isfinite(ratio)||ratio<=0||!isfinite(phase)||phase<0||
    !mulib_audio_core_finite(in,ib/sizeof *in))return false;
 for(size_t i=0;i<out_frames;i++){
  double p=phase+i*ratio;size_t a,b;double t;
  if(p>=(double)(in_frames-1)){a=b=in_frames-1;t=0;}
  else{a=(size_t)p;b=a+1;t=p-(double)a;}
  for(size_t j=0;j<ch;j++)out[i*ch+j]=mulib_audio_core_float((1-t)*(double)in[a*ch+j]+t*(double)in[b*ch+j]);
 }return true;
}
#ifdef __cplusplus
}
#endif
#endif
