/* MuLIB browser staging addition: indexeddb-store
 * Implemented subset: Bounded ordered key/value record store with borrowed keys and handles.
 * Remaining: Persistence, structured clone, key generators, transaction isolation.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_INDEXEDDB_STORE_H
#define MULIB_INDEXEDDB_STORE_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-indexeddb-key.h"
typedef struct {mulib_indexeddb_key_t key;uintptr_t value;} mulib_indexeddb_store_record;
typedef struct {mulib_indexeddb_store_record *v;size_t len,cap;} mulib_indexeddb_store_t;
static inline int mulib_indexeddb_store_put(mulib_indexeddb_store_t *s,mulib_indexeddb_key_t key,uintptr_t val,int overwrite){if(!s||!mulib_indexeddb_key_valid(key))return 0;size_t i=0;for(;i<s->len;i++){int c;if(!mulib_indexeddb_key_compare(s->v[i].key,key,&c))return 0;if(c==0){if(!overwrite)return 0;s->v[i].value=val;return 1;}if(c>0)break;}if(s->len>=s->cap)return 0;if(s->len>i)memmove(s->v+i+1,s->v+i,(s->len-i)*sizeof *s->v);s->v[i]=(mulib_indexeddb_store_record){key,val};s->len++;return 1;}
static inline int mulib_indexeddb_store_get(const mulib_indexeddb_store_t *s,mulib_indexeddb_key_t key,uintptr_t *out){if(!s||!out)return 0;for(size_t i=0;i<s->len;i++){int c;if(mulib_indexeddb_key_compare(s->v[i].key,key,&c)&&!c){*out=s->v[i].value;return 1;}}return 0;}
#endif /* MULIB_INDEXEDDB_STORE_H */
