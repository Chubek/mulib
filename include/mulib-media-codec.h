/** @file mulib-media-codec.h
 * Send/receive codec provider contract with explicit draining (NULL input).
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No bundled codec provider.
 */
#ifndef MULIB_MEDIA_CODEC_H
#define MULIB_MEDIA_CODEC_H
#include "mulib-media-core.h"
#include "mulib-media-packet.h"
#include "mulib-media-frame.h"
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
typedef struct {void *ctx;mulib_media_status (*send_packet)(void*,const mulib_media_packet*);mulib_media_status (*receive_frame)(void*,mulib_media_frame*);mulib_media_status (*send_frame)(void*,const mulib_media_frame*);mulib_media_status (*receive_packet)(void*,mulib_media_packet*);void (*flush)(void*);void (*close)(void*);} mulib_media_codec;
static inline mulib_media_status mulib_media_codec_decode_send(mulib_media_codec *c,const mulib_media_packet *p){if(!c|| (p&&!mulib_media_span_valid(p->bytes)))return MULIB_MEDIA_INVALID;return c->send_packet?c->send_packet(c->ctx,p):MULIB_MEDIA_UNSUPPORTED;}
/* Receive outputs must start empty, preventing accidental loss of live storage.
 * Ownership/release is provider-specific, including storage returned on error.
 * Callbacks must not reenter/mutate this codec. */
static inline mulib_media_status mulib_media_codec_decode_receive(mulib_media_codec *c,mulib_media_frame *f){if(!c||!f||f->data[0]||f->data[1]||f->data[2])return MULIB_MEDIA_INVALID;if(!c->receive_frame)return MULIB_MEDIA_UNSUPPORTED;mulib_media_status s=c->receive_frame(c->ctx,f);return s==MULIB_MEDIA_OK&&!mulib_media_frame_valid(f)?MULIB_MEDIA_IO:s;}
static inline mulib_media_status mulib_media_codec_encode_send(mulib_media_codec *c,const mulib_media_frame *f){return !c||(f&&!mulib_media_frame_valid(f))?MULIB_MEDIA_INVALID:!c->send_frame?MULIB_MEDIA_UNSUPPORTED:c->send_frame(c->ctx,f);}
static inline mulib_media_status mulib_media_codec_encode_receive(mulib_media_codec *c,mulib_media_packet *p){if(!c||!p||p->bytes.data||p->bytes.size)return MULIB_MEDIA_INVALID;if(!c->receive_packet)return MULIB_MEDIA_UNSUPPORTED;mulib_media_status s=c->receive_packet(c->ctx,p);return s==MULIB_MEDIA_OK&&!mulib_media_span_valid(p->bytes)?MULIB_MEDIA_IO:s;}
static inline void mulib_media_codec_close(mulib_media_codec *c){if(c){if(c->close)c->close(c->ctx);memset(c,0,sizeof *c);}}
#ifdef __cplusplus
}
#endif
#endif
