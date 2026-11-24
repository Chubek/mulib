/** @file mulib-media-demux.h
 * Demux provider lifecycle and checked dispatch.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Needs caller-supplied demux implementation (e.g. FFmpeg bridge).
 */
#ifndef MULIB_MEDIA_DEMUX_H
#define MULIB_MEDIA_DEMUX_H
#include "mulib-media-core.h"
#include "mulib-media-packet.h"
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
typedef struct {void *ctx;mulib_media_status (*next)(void*,mulib_media_packet*);mulib_media_status (*seek)(void*,int64_t);void (*close)(void*);} mulib_media_demux;
/* Provider must leave output empty on EOF/error, or supply a packet releasable
 * by its matching release callback. The wrapper does not assume malloc ownership. */
static inline mulib_media_status mulib_media_demux_next(mulib_media_demux *d,mulib_media_packet *p){if(!d||!d->next||!p||p->bytes.data||p->bytes.size)return MULIB_MEDIA_INVALID;mulib_media_status s=d->next(d->ctx,p);if(s==MULIB_MEDIA_OK&&!mulib_media_span_valid(p->bytes))return MULIB_MEDIA_IO;return s;}
static inline mulib_media_status mulib_media_demux_seek(mulib_media_demux *d,int64_t pts){return !d?MULIB_MEDIA_INVALID:!d->seek?MULIB_MEDIA_UNSUPPORTED:d->seek(d->ctx,pts);}
static inline void mulib_media_demux_close(mulib_media_demux *d){if(d){if(d->close)d->close(d->ctx);memset(d,0,sizeof *d);}}
#ifdef __cplusplus
}
#endif
#endif
