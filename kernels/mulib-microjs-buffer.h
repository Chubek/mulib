/* MuLIB browser staging addition: microjs-buffer
 * Implemented subset: Owned zeroed ArrayBuffer-like bytes, detach, little-endian uint32 access.
 * Remaining: Typed-array exotic objects, shared memory, resizable buffers and VM adapter.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_MICROJS_BUFFER_H
#define MULIB_MICROJS_BUFFER_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_buf bytes;int detached;} mulib_microjs_buffer_t;
static inline int mulib_microjs_buffer_init(mulib_microjs_buffer_t *b,size_t n){if(!b||b->bytes.data||b->detached)return 0;if(!muba_reserve(&b->bytes,n))return 0;if(n)memset(b->bytes.data,0,n);b->bytes.len=n;return 1;}
static inline void mulib_microjs_buffer_detach(mulib_microjs_buffer_t *b){if(b){muba_free(&b->bytes);b->detached=1;}}
static inline int mulib_microjs_buffer_read_u32(const mulib_microjs_buffer_t *b,size_t off,uint32_t *out){if(!b||!out||b->detached||off>b->bytes.len||b->bytes.len-off<4)return 0;const unsigned char *p=b->bytes.data+off;*out=(uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);return 1;}
static inline int mulib_microjs_buffer_write_u32(mulib_microjs_buffer_t *b,size_t off,uint32_t v){if(!b||b->detached||off>b->bytes.len||b->bytes.len-off<4)return 0;for(unsigned i=0;i<4;i++)b->bytes.data[off+i]=(unsigned char)(v>>(i*8));return 1;}
#endif /* MULIB_MICROJS_BUFFER_H */
