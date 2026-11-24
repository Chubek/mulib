/** @file mulib-media-isobmff.h
 * Bounds-checked ISO BMFF box traversal including extended sizes.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No sample tables, timestamps or container writing.
 */
#ifndef MULIB_MEDIA_ISOBMFF_H
#define MULIB_MEDIA_ISOBMFF_H
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
typedef struct {uint32_t type;uint64_t size;size_t header;mulib_media_bytes payload;} mulib_media_isobmff_box;
static inline bool mulib_media_isobmff_next(mulib_media_bytes b,size_t *offset,mulib_media_isobmff_box *out){if(!offset||!out||!b.data||*offset>b.size||b.size-*offset<8)return false;const uint8_t *p=b.data+*offset;uint64_t n=mulib_media_be32(p);size_t hdr=8;if(n==1){if(b.size-*offset<16)return false;n=mulib_media_be64(p+8);hdr=16;}else if(!n)n=b.size-*offset;uint32_t type=mulib_media_be32(p+4);if(type==0x75756964u)hdr+=16;if(n<hdr||n>b.size-*offset)return false;out->type=type;out->size=n;out->header=hdr;out->payload.data=p+hdr;out->payload.size=(size_t)n-hdr;*offset+=(size_t)n;return true;}
#ifdef __cplusplus
}
#endif
#endif
