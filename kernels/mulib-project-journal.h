/** @file mulib-project-journal.h
 * CRC32-framed journal append and valid-prefix recovery.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Memory serialization only; no fsync, atomic commit or transaction markers.
 */
#ifndef MULIB_PROJECT_JOURNAL_H
#define MULIB_PROJECT_JOURNAL_H
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
/* Little endian length + CRC32 + payload. CRC is corruption detection, not authentication. */
static inline uint32_t mulib_project_journal_crc(mulib_media_bytes b){uint32_t c=~0u;for(size_t i=0;i<b.size;i++){c^=b.data[i];for(int j=0;j<8;j++)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
static inline void mulib_project_journal_le(uint8_t *p,uint32_t x){for(unsigned j=0;j<4;j++)p[j]=(uint8_t)(x>>(8*j));}
static inline uint32_t mulib_project_journal_readle(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static inline bool mulib_project_journal_append(mulib_media_mutbytes b,size_t *used,mulib_media_bytes payload){if(!used||!b.data||!mulib_media_span_valid(payload)||payload.size>UINT32_MAX||*used>b.size||b.size-*used<8||payload.size>b.size-*used-8)return false;uint8_t *p=b.data+*used;uint32_t crc=mulib_project_journal_crc(payload);if(payload.size)memmove(p+8,payload.data,payload.size);mulib_project_journal_le(p,(uint32_t)payload.size);mulib_project_journal_le(p+4,crc);*used+=8+payload.size;return true;}
static inline bool mulib_project_journal_next(mulib_media_bytes b,size_t *off,mulib_media_bytes *out){if(!off||!out||!b.data||*off>b.size||b.size-*off<8)return false;const uint8_t *p=b.data+*off;size_t n=mulib_project_journal_readle(p);if(n>b.size-*off-8)return false;mulib_media_bytes v={p+8,n};if(mulib_project_journal_crc(v)!=mulib_project_journal_readle(p+4))return false;*off+=8+n;*out=v;return true;}
#ifdef __cplusplus
}
#endif
#endif
