/* MuLIB browser staging addition: indexeddb-key
 * Implemented subset: Non-NaN numeric and byte string keys with deterministic ordering.
 * Remaining: Date/binary/array types, UTF-16 ordering, key path extraction and serialization.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_INDEXEDDB_KEY_H
#define MULIB_INDEXEDDB_KEY_H
#include "detail/mulib-browser-additions-common.h"

typedef enum {mulib_indexeddb_key_NUMBER,mulib_indexeddb_key_STRING} mulib_indexeddb_key_kind;
typedef struct {mulib_indexeddb_key_kind kind;double number;muba_span string;} mulib_indexeddb_key_t;
static inline int mulib_indexeddb_key_valid(mulib_indexeddb_key_t k){return k.kind==mulib_indexeddb_key_NUMBER?!isnan(k.number):k.kind==mulib_indexeddb_key_STRING&&muba_span_ok(k.string);}
static inline int mulib_indexeddb_key_compare(mulib_indexeddb_key_t a,mulib_indexeddb_key_t b,int *out){if(!out||!mulib_indexeddb_key_valid(a)||!mulib_indexeddb_key_valid(b))return 0;if(a.kind!=b.kind){*out=a.kind<b.kind?-1:1;return 1;}if(a.kind==mulib_indexeddb_key_NUMBER){*out=(a.number>b.number)-(a.number<b.number);return 1;}size_t n=a.string.len<b.string.len?a.string.len:b.string.len;int c=n?memcmp(a.string.data,b.string.data,n):0;*out=c?(c>0?1:-1):(a.string.len>b.string.len)-(a.string.len<b.string.len);return 1;}
#endif /* MULIB_INDEXEDDB_KEY_H */
