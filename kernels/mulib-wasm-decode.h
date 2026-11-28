/* MuLIB browser staging addition: wasm-decode
 * Implemented subset: Wasm v1 preamble and checked unsigned LEB128 section envelope.
 * Remaining: Instruction grammar, signed LEB128, all section contents and proposals.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WASM_DECODE_H
#define MULIB_WASM_DECODE_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_wasm_decode_u32(muba_cursor *c,uint32_t *out){if(!c||!out)return 0;muba_cursor t=*c;uint32_t v=0;for(unsigned i=0;i<5;i++){muba_span s;if(!muba_take(&t,1,&s))return 0;unsigned b=s.data[0];if(i==4&&(b&0xf0))return 0;v|=(uint32_t)(b&127)<<(7*i);if(!(b&128)){*out=v;*c=t;return 1;}}return 0;}
static inline int mulib_wasm_decode_preamble(muba_cursor *c){if(!c)return 0;muba_cursor t=*c;muba_span s;static const unsigned char magic[]={0,97,115,109,1,0,0,0};if(!muba_take(&t,8,&s)||memcmp(s.data,magic,8))return 0;*c=t;return 1;}
static inline int mulib_wasm_decode_section(muba_cursor *c,unsigned *id,muba_span *body){if(!c||!id||!body)return 0;muba_cursor t=*c;muba_span s,b;uint32_t n;if(!muba_take(&t,1,&s)||!mulib_wasm_decode_u32(&t,&n)||!muba_take(&t,n,&b))return 0;*id=s.data[0];*body=b;*c=t;return 1;}
#endif /* MULIB_WASM_DECODE_H */
