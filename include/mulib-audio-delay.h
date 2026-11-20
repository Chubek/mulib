/** @file mulib-audio-delay.h
 * Allocation-free feedback delay ring.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Initialize state before per-sample call.
 */
#ifndef MULIB_AUDIO_DELAY_H
#define MULIB_AUDIO_DELAY_H
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
typedef struct {float *buffer;size_t length,cursor;float feedback,wet;} mulib_audio_delay;
/* buf has n writable float elements; its span must not overlap d. */
static inline bool mulib_audio_delay_init(mulib_audio_delay *d,float *buf,size_t n,float feedback,float wet){if(!d||!buf||!n||n>SIZE_MAX/sizeof *buf||!mulib_audio_core_disjoint(d,sizeof *d,buf,n*sizeof *buf)||!isfinite(feedback)||fabsf(feedback)>=1||!isfinite(wet)||wet<0||wet>1)return false;memset(buf,0,n*sizeof *buf);d->buffer=buf;d->length=n;d->cursor=0;d->feedback=feedback;d->wet=wet;return true;}
/* Unchecked realtime primitive: initialized d with cursor<length, valid ring,
 * finite x and exclusive access required. Overflow may poison ring state. */
static inline float mulib_audio_delay_tick(mulib_audio_delay *d,float x){float y=d->buffer[d->cursor];d->buffer[d->cursor]=mulib_audio_core_float((double)x+(double)y*d->feedback);if(++d->cursor==d->length)d->cursor=0;return mulib_audio_core_float((double)x*(1-d->wet)+(double)y*d->wet);}
#ifdef __cplusplus
}
#endif
#endif
