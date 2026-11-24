/** @file mulib-media-mux.h
 * Mux provider lifecycle with finalization guard.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Provider required.
 */
#ifndef MULIB_MEDIA_MUX_H
#define MULIB_MEDIA_MUX_H
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
typedef struct {void *ctx;mulib_media_status (*write)(void*,const mulib_media_packet*);mulib_media_status (*finish)(void*);void (*close)(void*);bool finished;} mulib_media_mux;
static inline mulib_media_status mulib_media_mux_write(mulib_media_mux *m,const mulib_media_packet *p){return !m||!m->write||m->finished||!p||!mulib_media_span_valid(p->bytes)?MULIB_MEDIA_INVALID:m->write(m->ctx,p);}
/* finished means finalization attempted, including failure: a partial trailer
 * cannot safely be retried. Providers are blocking; AGAIN is not retryable here.
 * Close/rollback remains the caller's responsibility after any error. */
static inline mulib_media_status mulib_media_mux_finish(mulib_media_mux *m){if(!m||!m->finish||m->finished)return MULIB_MEDIA_INVALID;m->finished=true;return m->finish(m->ctx);}
static inline void mulib_media_mux_close(mulib_media_mux *m){if(m){if(m->close)m->close(m->ctx);memset(m,0,sizeof *m);}}
#ifdef __cplusplus
}
#endif
#endif
