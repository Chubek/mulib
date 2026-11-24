/** @file mulib-audio-channelmap.h
 * Arbitrary interleaved channel mixing matrix.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Caller specifies layout matrix; disjoint input/output.
 */
#ifndef MULIB_AUDIO_CHANNELMAP_H
#define MULIB_AUDIO_CHANNELMAP_H
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
/* out[frames*nc], in[frames*ni], matrix[nc*ni], all float elements.
 * Output is disjoint from both read spans. Finite input/matrix required.
 * No writes on validation failure; overflowing sums produce signed infinity. */
static inline bool mulib_audio_channelmap(float *out,size_t nc,const float *in,size_t ni,size_t frames,const float *matrix){
 size_t ob,ib;
 if(!out||!in||!matrix||!nc||!ni||nc>32||ni>32||!mulib_audio_core_bytes(frames,nc,&ob)||!mulib_audio_core_bytes(frames,ni,&ib)||
    !mulib_audio_core_disjoint(out,ob,in,ib)||!mulib_audio_core_disjoint(out,ob,matrix,nc*ni*sizeof *matrix)||
    !mulib_audio_core_finite(matrix,nc*ni)||!mulib_audio_core_finite(in,ib/sizeof *in))return false;
 for(size_t f=0;f<frames;f++)for(size_t c=0;c<nc;c++){double s=0;for(size_t i=0;i<ni;i++)s+=(double)in[f*ni+i]*matrix[c*ni+i];out[f*nc+c]=mulib_audio_core_float(s);}
 return true;
}
#ifdef __cplusplus
}
#endif
#endif
