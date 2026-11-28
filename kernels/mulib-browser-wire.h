/* MuLIB browser staging addition: browser-wire
 * Implemented subset: Versioned little-endian event envelope with checked payload length, encode and decode.
 * Remaining: Schema evolution, canonical payloads, authentication, transport and incremental decoder.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_WIRE_H
#define MULIB_BROWSER_WIRE_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t seq,document,node;uint32_t channel,kind;muba_span payload;} mulib_browser_wire_event;
static inline void mulib_browser_wire_put(unsigned char *p,uint64_t x,unsigned n){for(unsigned i=0;i<n&&i<8;i++)p[i]=(unsigned char)(x>>(8*i));}
static inline uint64_t mulib_browser_wire_get(const unsigned char *p,unsigned n){uint64_t x=0;for(unsigned i=0;i<n&&i<8;i++)x|=(uint64_t)p[i]<<(8*i);return x;}
static inline int mulib_browser_wire_encode(mulib_browser_wire_event e,muba_buf *out){if(!out||!e.seq||!e.channel||!muba_span_ok(e.payload)||e.payload.len>UINT32_MAX)return 0;unsigned char h[40]={'M','U','B',1};mulib_browser_wire_put(h+4,e.seq,8);mulib_browser_wire_put(h+12,e.document,8);mulib_browser_wire_put(h+20,e.node,8);mulib_browser_wire_put(h+28,e.channel,4);mulib_browser_wire_put(h+32,e.kind,4);mulib_browser_wire_put(h+36,e.payload.len,4);muba_buf b={0};if(!muba_append(&b,(muba_span){h,40})||!muba_append(&b,e.payload)){muba_free(&b);return 0;}return muba_commit(out,&b);}
static inline int mulib_browser_wire_decode(muba_span s,size_t limit,mulib_browser_wire_event *out,size_t *used){if(!out||!used||!muba_span_ok(s)||s.len<40||memcmp(s.data,"MUB\1",4))return 0;size_t n=(size_t)mulib_browser_wire_get(s.data+36,4);if(n>limit||n>s.len-40)return 0;mulib_browser_wire_event e={mulib_browser_wire_get(s.data+4,8),mulib_browser_wire_get(s.data+12,8),mulib_browser_wire_get(s.data+20,8),(uint32_t)mulib_browser_wire_get(s.data+28,4),(uint32_t)mulib_browser_wire_get(s.data+32,4),{s.data+40,n}};if(!e.seq||!e.channel)return 0;*out=e;*used=40+n;return 1;}
#endif /* MULIB_BROWSER_WIRE_H */
