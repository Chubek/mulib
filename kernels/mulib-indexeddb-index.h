/* MuLIB browser staging addition: indexeddb-index
 * Implemented subset: Secondary index uniqueness validation against caller-supplied key list.
 * Remaining: MultiEntry, primary-key ordering, index maintenance and transactions.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_INDEXEDDB_INDEX_H
#define MULIB_INDEXEDDB_INDEX_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-indexeddb-key.h"
static inline int mulib_indexeddb_index_unique(const mulib_indexeddb_key_t *v,size_t n,mulib_indexeddb_key_t key){if((!v&&n)||!mulib_indexeddb_key_valid(key))return 0;for(size_t i=0;i<n;i++){int c;if(!mulib_indexeddb_key_compare(v[i],key,&c)||c==0)return 0;}return 1;}
#endif /* MULIB_INDEXEDDB_INDEX_H */
