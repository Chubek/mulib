/** @file mulib-audio-device.h
 * Audio provider lifecycle and partial-frame writes.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No native device provider; bridge existing miniaudio or platform audio.
 */
#ifndef MULIB_AUDIO_DEVICE_H
#define MULIB_AUDIO_DEVICE_H
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
typedef struct {void *ctx;unsigned channels,rate;mulib_media_status (*write)(void*,const float*,size_t,size_t*);mulib_media_status (*start)(void*);mulib_media_status (*stop)(void*);void (*close)(void*);bool running;} mulib_audio_device;
/* Provider owns the real device, thread affinity and blocking policy; calls
 * below are control/output operations, NOT realtime-callback-safe guarantees.
 * No native device is opened here. Context is move-only/caller-serialized.
 * PCM limits match mulib-audio.h. Provider must not retain borrowed samples. */
static inline bool mulib_audio_device_valid(const mulib_audio_device *d){return d&&d->ctx&&d->channels&&d->channels<=8&&d->rate&&d->rate<=768000&&d->write&&d->start&&d->stop&&d->close;}
static inline mulib_media_status mulib_audio_device_status(mulib_media_status s){return s<=MULIB_MEDIA_OK&&s>=MULIB_MEDIA_CANCELLED?s:MULIB_MEDIA_IO;}
static inline mulib_media_status mulib_audio_device_start(mulib_audio_device *d){if(!mulib_audio_device_valid(d)||d->running)return MULIB_MEDIA_INVALID;mulib_media_status s=mulib_audio_device_status(d->start(d->ctx));if(s==MULIB_MEDIA_OK)d->running=true;return s;}
/* n/written count FRAMES. p has n*channels finite floats, disjoint from d and
 * written. A provider may report partial progress even on error; impossible
 * progress is replaced by zero/IO. Validation failure changes nothing. */
static inline mulib_media_status mulib_audio_device_write(mulib_audio_device *d,const float *p,size_t n,size_t *written){
 size_t bytes;
 if(!mulib_audio_device_valid(d)||!d->running||!written||(!p&&n)||!mulib_audio_core_bytes(n,d->channels,&bytes)||
    !mulib_audio_core_disjoint(d,sizeof *d,p,bytes)||!mulib_audio_core_disjoint(written,sizeof *written,d,sizeof *d)||
    !mulib_audio_core_disjoint(written,sizeof *written,p,bytes)||!mulib_audio_core_finite(p,bytes/sizeof *p))return MULIB_MEDIA_INVALID;
 size_t done=0;mulib_media_status s=mulib_audio_device_status(d->write(d->ctx,p,n,&done));
 if(done>n){*written=0;return MULIB_MEDIA_IO;}*written=done;return s;
}
static inline mulib_media_status mulib_audio_device_stop(mulib_audio_device *d){if(!mulib_audio_device_valid(d)||!d->running)return MULIB_MEDIA_INVALID;mulib_media_status s=mulib_audio_device_status(d->stop(d->ctx));if(s==MULIB_MEDIA_OK)d->running=false;return s;}
static inline void mulib_audio_device_close(mulib_audio_device *d){if(d){if(d->running&&d->stop)d->stop(d->ctx);if(d->close)d->close(d->ctx);memset(d,0,sizeof *d);}}
#ifdef __cplusplus
}
#endif
#endif
