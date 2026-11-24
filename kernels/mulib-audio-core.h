/** @file mulib-audio-core.h
 * Checked borrowed PCM spans for the small audio DSP kernels.
 * Frames are samples per channel; capacities are float elements or bytes as
 * named. Interleaved samples use the same layout as mulib_audio_pcm_t in
 * mulib-audio.h; these helpers never allocate or acquire ownership.
 * Finite HDR samples are accepted; no clipping/normalization is implicit.
 * Subnormal arithmetic follows the host floating-point environment.
 */
#ifndef MULIB_AUDIO_CORE_H
#define MULIB_AUDIO_CORE_H
#include "mulib-media-core.h"
#ifdef __cplusplus
extern "C" {
#endif
static inline bool mulib_audio_core_bytes(size_t frames,size_t channels,size_t *bytes){
 size_t count;
 return channels&&mulib_media_size_mul(frames,channels,&count)&&mulib_media_size_mul(count,sizeof(float),bytes);
}
/* Flat-address conservative alias check. Invalid/wrapping spans are rejected;
 * this is not proof of allocation capacity, lifetime or pointer provenance. */
static inline bool mulib_audio_core_disjoint(const void *a,size_t an,const void *b,size_t bn){
 if(!an||!bn)return true;
 if(!a||!b||an>UINTPTR_MAX||bn>UINTPTR_MAX)return false;
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 if(x>UINTPTR_MAX-an||y>UINTPTR_MAX-bn)return false;
 return x<=y?an<=y-x:bn<=x-y;
}
static inline bool mulib_audio_core_finite(const float *p,size_t count){
 if((!p&&count)||count>SIZE_MAX/sizeof *p)return false;
 for(size_t i=0;i<count;i++)if(!isfinite(p[i]))return false;
 return true;
}
/* DSP output policy: preserve NaN as NaN and overflow as signed infinity,
 * rather than perform an out-of-range double-to-float conversion. */
static inline float mulib_audio_core_float(double x){
 return x>FLT_MAX?INFINITY:x< -FLT_MAX?-INFINITY:(float)x;
}
#ifdef __cplusplus
}
#endif
#endif
