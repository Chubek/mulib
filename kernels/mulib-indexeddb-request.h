/* MuLIB browser staging addition: indexeddb-request
 * Implemented subset: One-shot pending/result/error settlement.
 * Remaining: Asynchronous events, request reuse, transaction association and JS bindings.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_INDEXEDDB_REQUEST_H
#define MULIB_INDEXEDDB_REQUEST_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {int done,error;uintptr_t result;} mulib_indexeddb_request_t;
static inline int mulib_indexeddb_request_settle(mulib_indexeddb_request_t *r,int error,uintptr_t result){if(!r||r->done)return 0;*r=(mulib_indexeddb_request_t){1,!!error,result};return 1;}
#endif /* MULIB_INDEXEDDB_REQUEST_H */
