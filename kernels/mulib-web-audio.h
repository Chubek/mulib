/* MuLIB browser staging addition: web-audio
 * Implemented subset: In-place finite PCM gain with optional clipping.
 * Remaining: Audio graph, sample scheduling, automation, worklets and device output.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_AUDIO_H
#define MULIB_WEB_AUDIO_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_web_audio_gain(float *pcm,size_t n,float gain,int clip){if((!pcm&&n)||!isfinite(gain))return 0;for(size_t i=0;i<n;i++)if(!isfinite(pcm[i])||(!clip&&!isfinite(pcm[i]*gain)))return 0;for(size_t i=0;i<n;i++){double x=(double)pcm[i]*gain;pcm[i]=(float)(clip?fmin(1,fmax(-1,x)):x);}return 1;}
#endif /* MULIB_WEB_AUDIO_H */
