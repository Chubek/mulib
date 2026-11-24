/** @file mulib-media-export.h
 * Cancellable packet remux loop with release and finalization.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Provider callbacks must be blocking; no encode or atomic filesystem commit.
 */
#ifndef MULIB_MEDIA_EXPORT_H
#define MULIB_MEDIA_EXPORT_H
#include "mulib-media-core.h"
#include "mulib-media-demux.h"
#include "mulib-media-mux.h"
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
typedef bool (*mulib_media_export_progress)(void*,size_t);
/* Blocking packet remux loop; progress false cancels. Caller owns rollback and
 * providers. release must match the demux provider, accept any returned nonempty
 * packet (even on EOF/error), and not release any unrelated resources. */
static inline mulib_media_status mulib_media_export_remux(mulib_media_demux *d,mulib_media_mux *m,mulib_media_export_progress progress,void *ctx,void (*release)(mulib_media_packet*)){if(!d||!d->next||!m||!m->write||!m->finish||m->finished||!release)return MULIB_MEDIA_INVALID;size_t n=0;for(;;){if(progress&&!progress(ctx,n))return MULIB_MEDIA_CANCELLED;mulib_media_packet p={0};mulib_media_status r=mulib_media_demux_next(d,&p);if(r!=MULIB_MEDIA_OK){if(p.bytes.data||p.bytes.size)release(&p);return r==MULIB_MEDIA_EOF?mulib_media_mux_finish(m):r;}r=mulib_media_mux_write(m,&p);release(&p);if(r!=MULIB_MEDIA_OK)return r;if(n==SIZE_MAX)return MULIB_MEDIA_RANGE;n++;}}
#ifdef __cplusplus
}
#endif
#endif
