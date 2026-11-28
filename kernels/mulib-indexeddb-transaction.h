/* MuLIB browser staging addition: indexeddb-transaction
 * Implemented subset: Explicit transaction state and commit/rollback callback dispatch.
 * Remaining: ACID storage, scope locking, scheduling and event loop rules.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_INDEXEDDB_TRANSACTION_H
#define MULIB_INDEXEDDB_TRANSACTION_H
#include "detail/mulib-browser-additions-common.h"

typedef enum {mulib_indexeddb_transaction_ACTIVE,mulib_indexeddb_transaction_COMMITTED,mulib_indexeddb_transaction_ABORTED} mulib_indexeddb_transaction_state;
typedef struct {mulib_indexeddb_transaction_state state;int (*commit)(void *);void (*rollback)(void *);void *ctx;int busy;} mulib_indexeddb_transaction_t;
static inline int mulib_indexeddb_transaction_finish(mulib_indexeddb_transaction_t *t,int commit){if(!t||t->busy||t->state!=mulib_indexeddb_transaction_ACTIVE)return 0;t->busy=1;int ok=commit&&t->commit&&t->commit(t->ctx);t->state=ok?mulib_indexeddb_transaction_COMMITTED:mulib_indexeddb_transaction_ABORTED;if(!ok&&t->rollback)t->rollback(t->ctx);t->busy=0;return commit?ok:1;}
#endif /* MULIB_INDEXEDDB_TRANSACTION_H */
