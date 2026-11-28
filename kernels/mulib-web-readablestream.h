/* MuLIB browser staging addition: web-readablestream
 * Implemented subset: Bounded owned byte FIFO with close/error and pull semantics.
 * Remaining: JS stream objects, BYOB, locks, tee and promise backpressure.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_READABLESTREAM_H
#define MULIB_WEB_READABLESTREAM_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_buf b;size_t limit;int closed,error;} mulib_web_readablestream_t;
static inline int mulib_web_readablestream_push(mulib_web_readablestream_t *s,muba_span v){if(!s||s->closed||s->error||s->b.len>s->limit||v.len>s->limit-s->b.len)return 0;return muba_append(&s->b,v);}
static inline int mulib_web_readablestream_read(mulib_web_readablestream_t *s,unsigned char *out,size_t cap,size_t *n){if(!s||!n||(!out&&cap)||s->error)return -1;size_t k=s->b.len<cap?s->b.len:cap;if(k){memcpy(out,s->b.data,k);memmove(s->b.data,s->b.data+k,s->b.len-k);s->b.len-=k;}*n=k;return k?1:s->closed?0:2;}
static inline void mulib_web_readablestream_close(mulib_web_readablestream_t *s){if(s)s->closed=1;}
static inline void mulib_web_readablestream_free(mulib_web_readablestream_t *s){if(s){muba_free(&s->b);s->closed=1;}}
#endif /* MULIB_WEB_READABLESTREAM_H */
