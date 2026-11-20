/** @file mulib-audio-timestretch.h
 * Offline windowed overlap-add time stretching.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No WSOLA alignment or phase vocoder; pitch preservation is approximate, mono only.
 */
#ifndef MULIB_AUDIO_TIMESTRETCH_H
#define MULIB_AUDIO_TIMESTRETCH_H
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
/* Offline weighted overlap-add. out/norm each have cap floats; in has n finite
 * floats. All spans and written must be disjoint. Incomplete final windows are
 * discarded. No writes on validation failure. Work is frames*window, not RT.
 * Norm contains accumulated window weights; output uses an incremental weighted
 * mean to avoid overflowing intermediate sums for finite HDR samples. */
static inline bool mulib_audio_timestretch_ola(float *out,float *norm,size_t cap,const float *in,size_t n,size_t window,size_t analysis_hop,size_t synthesis_hop,size_t *written){
 size_t ob,ib;
 if(!out||!norm||!in||!written||window<4||!analysis_hop||analysis_hop>=window||!synthesis_hop||synthesis_hop>=window||n<window||
    !mulib_audio_core_bytes(cap,1,&ob)||!mulib_audio_core_bytes(n,1,&ib)||
    !mulib_audio_core_disjoint(out,ob,norm,ob)||!mulib_audio_core_disjoint(out,ob,in,ib)||!mulib_audio_core_disjoint(norm,ob,in,ib)||
    !mulib_audio_core_disjoint(written,sizeof *written,out,ob)||!mulib_audio_core_disjoint(written,sizeof *written,norm,ob)||
    !mulib_audio_core_disjoint(written,sizeof *written,in,ib))return false;
 size_t frames=1+(n-window)/analysis_hop,len;
 if(!mulib_media_size_mul(frames-1,synthesis_hop,&len)||!mulib_media_size_add(len,window,&len)||len>cap||!mulib_audio_core_finite(in,n))return false;
 memset(out,0,len*sizeof *out);memset(norm,0,len*sizeof *norm);
 for(size_t f=0;f<frames;f++)for(size_t i=0;i<window;i++){
  double w=.5-.5*cos(6.283185307179586*((double)i+.5)/(double)window);size_t k=f*synthesis_hop+i;
  double weight=(double)norm[k]+w;
  if(weight>0){out[k]=mulib_audio_core_float(((double)norm[k]/weight)*out[k]+(w/weight)*in[f*analysis_hop+i]);norm[k]=(float)weight;}
 }
 *written=len;return true;
}
#ifdef __cplusplus
}
#endif
#endif
