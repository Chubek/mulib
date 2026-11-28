/* MuLIB browser staging addition: web-blob
 * Implemented subset: Owned byte blob, slice with signed indices and explicit free.
 * Remaining: Type normalization, JS Blob constructors, streaming and file backing.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_BLOB_H
#define MULIB_WEB_BLOB_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_buf bytes;} mulib_web_blob_t;
static inline int mulib_web_blob_init(mulib_web_blob_t *b,muba_span v){return b&&!b->bytes.data&&muba_append(&b->bytes,v);}
static inline size_t mulib_web_blob_index(int64_t i,size_t n){if(i>=0)return (uint64_t)i>n?n:(size_t)i;uint64_t mag=(uint64_t)(-(i+1))+1;return mag>n?0:n-(size_t)mag;}
static inline int mulib_web_blob_slice(const mulib_web_blob_t *b,int64_t a,int64_t z,mulib_web_blob_t *out){if(!b||!out||b==out||out->bytes.data)return 0;size_t x=mulib_web_blob_index(a,b->bytes.len),y=mulib_web_blob_index(z,b->bytes.len);return muba_append(&out->bytes,(muba_span){b->bytes.data?b->bytes.data+x:NULL,y>x?y-x:0});}
static inline void mulib_web_blob_free(mulib_web_blob_t *b){if(b)muba_free(&b->bytes);}
#endif /* MULIB_WEB_BLOB_H */
