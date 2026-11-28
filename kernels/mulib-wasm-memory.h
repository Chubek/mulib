/* MuLIB browser staging addition: wasm-memory
 * Implemented subset: Owned linear memory with 64KiB pages, bounded grow and checked load/store.
 * Remaining: Shared memory, memory64, traps and engine integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WASM_MEMORY_H
#define MULIB_WASM_MEMORY_H
#include "detail/mulib-browser-additions-common.h"

#define MULIB_WASM_MEMORY_PAGE_SIZE 65536u
typedef struct {muba_buf b;uint32_t pages,max;} mulib_wasm_memory_t;
static inline int mulib_wasm_memory_grow(mulib_wasm_memory_t *m,uint32_t n,uint32_t *old){if(!m||!old||m->pages>m->max||n>m->max-m->pages)return 0;size_t bytes;if(!muba_mul_size((size_t)(m->pages+n),MULIB_WASM_MEMORY_PAGE_SIZE,&bytes)||!muba_reserve(&m->b,bytes))return 0;if(bytes>m->b.len)memset(m->b.data+m->b.len,0,bytes-m->b.len);*old=m->pages;m->pages+=n;m->b.len=bytes;return 1;}
static inline int mulib_wasm_memory_read(const mulib_wasm_memory_t *m,size_t addr,void *out,size_t n){if(!m||(!out&&n)||addr>m->b.len||n>m->b.len-addr)return 0;if(n)memmove(out,m->b.data+addr,n);return 1;}
static inline int mulib_wasm_memory_write(mulib_wasm_memory_t *m,size_t addr,const void *src,size_t n){if(!m||(!src&&n)||addr>m->b.len||n>m->b.len-addr)return 0;if(n)memmove(m->b.data+addr,src,n);return 1;}
static inline void mulib_wasm_memory_free(mulib_wasm_memory_t *m){if(m){muba_free(&m->b);m->pages=0;}}
#endif /* MULIB_WASM_MEMORY_H */
