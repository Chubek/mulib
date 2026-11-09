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
int mulib_eda_transaction_begin(mulib_eda_transaction_t *t, mulib_eda_design_db_t *target);
int mulib_eda_transaction_commit(mulib_eda_transaction_t *t);
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
    if (!t || !db || t->active || db->count > db->capacity ||
        db->count > SIZE_MAX / sizeof *db->items || (db->count && !db->items))
        return 0;
    memset(t, 0, sizeof *t);
    if (db->count) {
        t->staged.items = malloc(db->count * sizeof *db->items);
        if (!t->staged.items)
            return 0;
        memcpy(t->staged.items, db->items, db->count * sizeof *db->items);
    }
    t->staged.count = db->count;
    t->staged.capacity = db->count;
    t->staged.epoch = db->epoch;
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
