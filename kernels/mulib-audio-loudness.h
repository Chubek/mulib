/** @file mulib-audio-loudness.h
 * Streaming RMS dBFS and sample-peak measurement.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Not LUFS/ITU BS.1770 or oversampled true peak.
 */
#ifndef MULIB_AUDIO_LOUDNESS_H
#define MULIB_AUDIO_LOUDNESS_H
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
typedef struct {double sum_squares,peak;uint64_t count;} mulib_audio_loudness;
/* Zero-initialize m. a contains n finite scalar samples (all channels may be
 * accumulated together); no mutation on invalid state/input or overflow. */
static inline bool mulib_audio_loudness_update(mulib_audio_loudness *m,const float *a,size_t n){if(!m||(!a&&n)||n>SIZE_MAX/sizeof *a||n>UINT64_MAX-m->count||!isfinite(m->sum_squares)||m->sum_squares<0||!isfinite(m->peak)||m->peak<0)return false;mulib_audio_loudness s=*m;for(size_t i=0;i<n;i++){if(!isfinite(a[i]))return false;double x=a[i];s.sum_squares+=x*x;s.peak=fmax(s.peak,fabs(x));}if(!isfinite(s.sum_squares))return false;s.count+=n;*m=s;return true;}
static inline double mulib_audio_loudness_rms_db(mulib_audio_loudness m){return m.count&&m.sum_squares>0?10*log10(m.sum_squares/m.count):-INFINITY;}
static inline double mulib_audio_loudness_peak_db(mulib_audio_loudness m){return m.peak>0?20*log10(m.peak):-INFINITY;}
#ifdef __cplusplus
}
#endif
#endif
