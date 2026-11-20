/** @file mulib-media-matroska.h
 * EBML variable integers and bounded element traversal.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Unknown-sized element consumes remaining caller-supplied scope; no block/lacing support.
 */
#ifndef MULIB_MEDIA_MATROSKA_H
#define MULIB_MEDIA_MATROSKA_H
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
static inline bool mulib_media_matroska_vint(mulib_media_bytes b,size_t *off,bool id,uint64_t *v,bool *unknown){if(!off||!v||!b.data||*off>=b.size)return false;uint8_t c=b.data[*off],mask=128;unsigned n=1;while(mask&&!(c&mask)){mask>>=1;n++;}if(!mask||n>b.size-*off||(id&&n>4))return false;uint64_t x=id?c:c&(mask-1);for(unsigned i=1;i<n;i++)x=(x<<8)|b.data[*off+i];*v=x;if(unknown)*unknown=!id&&x==((UINT64_C(1)<<(7*n))-1);*off+=n;return true;}
typedef struct {uint64_t id;mulib_media_bytes payload;bool unknown;} mulib_media_matroska_element;
static inline bool mulib_media_matroska_next(mulib_media_bytes b,size_t *off,mulib_media_matroska_element *out){if(!off||!out)return false;size_t i=*off;uint64_t id,n;bool u;if(!mulib_media_matroska_vint(b,&i,true,&id,NULL)||!mulib_media_matroska_vint(b,&i,false,&n,&u)||(!u&&n>b.size-i))return false;if(u)n=b.size-i;out->id=id;out->payload.data=b.data+i;out->payload.size=(size_t)n;out->unknown=u;*off=i+(size_t)n;return true;}
#ifdef __cplusplus
}
#endif
#endif
