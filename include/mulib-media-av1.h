/** @file mulib-media-av1.h
 * AV1 low-overhead OBU framing and LEB128 sizes.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No AV1 encoder/decoder.
 */
#ifndef MULIB_MEDIA_AV1_H
#define MULIB_MEDIA_AV1_H
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
typedef struct {unsigned type,temporal,spatial;mulib_media_bytes payload;} mulib_media_av1_obu;
static inline bool mulib_media_av1_leb(mulib_media_bytes b,size_t *p,uint64_t *out){if(!b.data||!p||!out)return false;size_t i=*p;uint64_t v=0;for(unsigned k=0;k<8;k++){if(i>=b.size)return false;unsigned c=b.data[i++];v|=(uint64_t)(c&127)<<(7*k);if(!(c&128)){*out=v;*p=i;return true;}}return false;}
static inline bool mulib_media_av1_next(mulib_media_bytes b,size_t *off,mulib_media_av1_obu *out){if(!off||!out||!b.data||*off>=b.size)return false;size_t i=*off;unsigned h=b.data[i++];if(h&129)return false;mulib_media_av1_obu o={0};o.type=(h>>3)&15;if(h&4){if(i==b.size)return false;unsigned e=b.data[i++];if(e&7)return false;o.temporal=e>>5;o.spatial=(e>>3)&3;}uint64_t n=b.size-i;if((h&2)&&!mulib_media_av1_leb(b,&i,&n))return false;if(n>b.size-i)return false;o.payload.data=b.data+i;o.payload.size=(size_t)n;*off=i+(size_t)n;*out=o;return true;}
#ifdef __cplusplus
}
#endif
#endif
