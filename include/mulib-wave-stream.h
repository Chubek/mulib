/* MuLIB / Amb3r: wave-stream. C11 single-header kernel.
 * Define MULIB_IMPL_WAVE_STREAM in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_WAVE_STREAM_H
#define MULIB_WAVE_STREAM_H
#include "mulib-amb3r-base.h"
#include "mulib-wave-store.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {int (*send)(void *,const ma_wave_sample *);void *context;double last_time;int has_last;} ma_wave_stream;
int mulib_wave_stream_push(ma_wave_stream *s,const ma_wave_sample *samples,size_t n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WAVE_STREAM) && !defined(MULIB_WAVE_STREAM_H_IMPLEMENTED)
#define MULIB_WAVE_STREAM_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_wave_stream_push(ma_wave_stream *s,const ma_wave_sample *samples,size_t n){if(!s||(!samples&&n)||!s->send)return 0;for(size_t i=0;i<n;i++){if(!isfinite(samples[i].time)||!isfinite(samples[i].value)||(s->has_last&&samples[i].time<=s->last_time)||!s->send(s->context,samples+i))return 0;s->last_time=samples[i].time;s->has_last=1;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
