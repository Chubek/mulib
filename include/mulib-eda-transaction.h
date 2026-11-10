/** @file mulib-eda-transaction.h
 * @brief Optimistic owned-snapshot transaction over eda-design-db.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_TRANSACTION in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_TRANSACTION_H
#define MULIB_EDA_TRANSACTION_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-eda-design-db.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eda_transaction {
    mulib_eda_design_db_t *target, staged;
    uint64_t expected_epoch;
    int active;
} mulib_eda_transaction_t;
/** Begin an independent owned snapshot of target; return 1 on success.
 * t must be zero-initialized or inactive with empty staged storage. Rejects
 * NULL, active transactions, nonempty staged storage and target == &t->staged.
 * On invalid input, size overflow or allocation failure, both objects remain
 * unchanged. Uses eda-design-db cloning: O(target->count) time and storage.
 * The target is borrowed and must remain live, at the same address, until
 * commit/rollback. Do not free/reinitialize it or alter its epoch meanwhile.
 * Mutate staged through eda-design-db APIs; do not shallow-copy owned arrays.
 * All access requires caller serialization; no nested transactions or locks.
 */
int mulib_eda_transaction_begin(mulib_eda_transaction_t *t, mulib_eda_design_db_t *target);
/** Publish staged storage if the target epoch still matches; return 1.
 * Allocation-free; advances target epoch once, even for an unedited snapshot,
 * and invalidates pointers into the old target array. Returns 0 on inactive,
 * stale or epoch-exhausted transactions without changing target or staged
 * state. Active transactions remain active on failure for inspection/rollback.
 * Successful
 * commit transfers staged ownership to target and makes t reusable.
 */
int mulib_eda_transaction_commit(mulib_eda_transaction_t *t);
/** Free staged storage, detach the borrowed target and mark t inactive.
 * NULL and repeated rollback are harmless for initialized transactions.
 * Does not access or mutate the target; invalidates staged-array pointers.
 */
void mulib_eda_transaction_rollback(mulib_eda_transaction_t *t);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_TRANSACTION_H */
#if defined(MULIB_IMPL_EDA_TRANSACTION) && !defined(MULIB_EDA_TRANSACTION_H_IMPLEMENTED)
#define MULIB_EDA_TRANSACTION_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_transaction_begin(mulib_eda_transaction_t *t, mulib_eda_design_db_t *db) {
    if (!t || !db || db == &t->staged || t->active || t->staged.items ||
        t->staged.count || t->staged.capacity)
        return 0;
    mulib_eda_design_db_t staged = {0};
    if (!mulib_eda_db_clone(db, &staged))
        return 0;
    t->staged = staged;
    t->target = db;
    t->expected_epoch = db->epoch;
    t->active = 1;
    return 1;
}
void mulib_eda_transaction_rollback(mulib_eda_transaction_t *t) {
    if (!t)
        return;
    mulib_eda_db_free(&t->staged);
    t->target = NULL;
    t->active = 0;
}
int mulib_eda_transaction_commit(mulib_eda_transaction_t *t) {
    if (!t || !t->active || !t->target || t->target->epoch != t->expected_epoch ||
        t->expected_epoch == UINT64_MAX)
        return 0;
    t->staged.epoch = t->expected_epoch + 1;
    mulib_eda_design_db_t tmp = *t->target;
    *t->target = t->staged;
    memset(&t->staged, 0, sizeof t->staged);
    free(tmp.items);
    t->active = 0;
    t->target = NULL;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_EDA_TRANSACTION */
