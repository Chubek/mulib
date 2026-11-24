/** @file mulib-media-packet.h
 * Deep copy and release of encoded packets.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Release only cloned packets.
 */
#ifndef MULIB_MEDIA_PACKET_H
#define MULIB_MEDIA_PACKET_H
#include "mulib-media-core.h"
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
typedef struct { mulib_media_bytes bytes;int64_t pts,dts,duration;uint32_t stream;bool keyframe; } mulib_media_packet;
/* out must have an empty byte span. Owned clones must not be shallow copied;
 * release only matching clones, never borrowed provider packets. */
static inline bool mulib_media_packet_clone(mulib_media_packet *out,const mulib_media_packet *in){if(!out||!in||out==in||out->bytes.data||out->bytes.size||!mulib_media_span_valid(in->bytes))return false;uint8_t *p=NULL;if(in->bytes.size){p=(uint8_t*)malloc(in->bytes.size);if(!p)return false;memcpy(p,in->bytes.data,in->bytes.size);}*out=*in;out->bytes.data=p;return true;}
static inline void mulib_media_packet_release(mulib_media_packet *p){if(p){free((void*)p->bytes.data);memset(p,0,sizeof *p);}}
#ifdef __cplusplus
}
#endif
#endif
