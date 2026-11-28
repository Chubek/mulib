/* MuLIB browser staging addition: hpack
 * Implemented subset: HPACK prefix integer codec with overflow/truncation checks.
 * Remaining: Static/dynamic tables, Huffman and header block processing.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_HPACK_H
#define MULIB_HPACK_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_hpack_decode(muba_span s,unsigned prefix,uint64_t *out,size_t *used){if(!out||!used||!muba_span_ok(s)||!s.len||!prefix||prefix>8)return 0;uint64_t mask=(1u<<prefix)-1,v=s.data[0]&mask;if(v<mask){*out=v;*used=1;return 1;}unsigned shift=0;for(size_t i=1;i<s.len;i++){uint64_t x=s.data[i]&127;if(shift>=64||x>(UINT64_MAX-v)>>shift)return 0;v+=x<<shift;if(!(s.data[i]&128)){*out=v;*used=i+1;return 1;}shift+=7;}return 0;}
static inline int mulib_hpack_encode(uint64_t v,unsigned prefix,unsigned char flags,muba_buf *out){if(!out||!prefix||prefix>8)return 0;unsigned mask=(1u<<prefix)-1;if(flags&mask)return 0;muba_buf b={0};if(v<mask){if(!muba_byte(&b,(unsigned char)(flags|v)))goto fail;}else{if(!muba_byte(&b,(unsigned char)(flags|mask)))goto fail;v-=mask;while(v>=128){if(!muba_byte(&b,(unsigned char)((v&127)|128)))goto fail;v>>=7;}if(!muba_byte(&b,(unsigned char)v))goto fail;}return muba_commit(out,&b);fail:muba_free(&b);return 0;}
#endif /* MULIB_HPACK_H */
