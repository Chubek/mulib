/* MuLIB browser staging addition: indexeddb-cursor
 * Implemented subset: Forward/reverse cursor over an immutable ordered store snapshot.
 * Remaining: Live transaction cursor, key ranges, continue and update/delete.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_INDEXEDDB_CURSOR_H
#define MULIB_INDEXEDDB_CURSOR_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-indexeddb-store.h"
typedef struct {const mulib_indexeddb_store_record *v;size_t len,pos;int reverse;} mulib_indexeddb_cursor_t;
static inline const mulib_indexeddb_store_record *mulib_indexeddb_cursor_next(mulib_indexeddb_cursor_t *c){if(!c||!c->v||c->pos>=c->len)return NULL;size_t i=c->reverse?c->len-1-c->pos:c->pos;c->pos++;return c->v+i;}
#endif /* MULIB_INDEXEDDB_CURSOR_H */
