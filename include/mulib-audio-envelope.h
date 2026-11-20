/** @file mulib-audio-envelope.h
 * Linear or power-interpolated per-frame gain envelope.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_AUDIO_ENVELOPE_H
#define MULIB_AUDIO_ENVELOPE_H
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
/* p has frames*ch finite floats. A single frame uses the terminal gain.
 * Equal-power mode requires nonnegative gains; overflow outputs infinity. */
static inline bool mulib_audio_envelope(float *p,size_t frames,size_t ch,float from,float to,bool equal_power){
 size_t bytes;
 if(!p||!mulib_audio_core_bytes(frames,ch,&bytes)||!isfinite(from)||!isfinite(to)||
    (equal_power&&(from<0||to<0))||!mulib_audio_core_finite(p,bytes/sizeof *p))return false;
 for(size_t i=0;i<frames;i++){double t=frames>1?(double)i/(double)(frames-1):1;
  double g=equal_power?sqrt((1-t)*(double)from*from+t*(double)to*to):(1-t)*(double)from+t*(double)to;
  for(size_t c=0;c<ch;c++)p[i*ch+c]=mulib_audio_core_float((double)p[i*ch+c]*g);
 }return true;
}
#ifdef __cplusplus
}
#endif
#endif
