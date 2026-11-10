/* Focused executable regression for mulib-eda-transaction.h. */
#define MULIB_IMPL_EDA_DESIGN_DB
#define MULIB_IMPL_EDA_TRANSACTION
#include "mulib-eda-transaction.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
static void invalid_ownership(void) {
    mulib_eda_design_db_t db = {0};
    mulib_eda_transaction_t t = {0};
    unsigned char before[sizeof t];
    memcpy(before, &t, sizeof t);
    assert(!mulib_eda_transaction_begin(&t, &t.staged));
    assert(!memcmp(before, &t, sizeof t));

    /* Inactive staged storage is still owned: begin must not leak it. */
    assert(mulib_eda_db_put(&t.staged, (mulib_eda_db_record_t){1, 1, 5}));
    memcpy(before, &t, sizeof t);
    assert(!mulib_eda_transaction_begin(&t, &db));
    assert(!memcmp(before, &t, sizeof t));
    mulib_eda_transaction_rollback(&t);
    assert(!t.staged.items && !t.active);

    /* Clone's source-shape checks must also apply to begin. */
    db.count = 1;
    assert(!mulib_eda_transaction_begin(&t, &db));
    db.capacity = 1;
    assert(!mulib_eda_transaction_begin(&t, &db));
    db.count = db.capacity = 0;
    assert(!mulib_eda_transaction_begin(NULL, &db));
    assert(!mulib_eda_transaction_begin(&t, NULL));
    assert(!mulib_eda_transaction_commit(&t));
    mulib_eda_transaction_rollback(NULL);
}

static void multi_record_edits(void) {
    mulib_eda_design_db_t db = {0};
    mulib_eda_transaction_t t = {0}, other = {0};
    mulib_eda_db_record_t record;
    assert(mulib_eda_db_put(&db, (mulib_eda_db_record_t){1, 1, 5}));
    assert(mulib_eda_db_put(&db, (mulib_eda_db_record_t){2, 2, 10}));
    uint64_t epoch = db.epoch;
    assert(mulib_eda_transaction_begin(&t, &db));
    assert(!mulib_eda_transaction_begin(&t, &db));
    assert(t.staged.items != db.items);
    assert(mulib_eda_db_put(&t.staged, (mulib_eda_db_record_t){1, 1, 7}));
    assert(mulib_eda_db_delete(&t.staged, 2));
    assert(mulib_eda_db_put(&t.staged, (mulib_eda_db_record_t){3, 3, 15}));
    assert(db.epoch == epoch && db.count == 2);
    assert(mulib_eda_db_get(&db, 1, &record) && record.value == 5);
    assert(mulib_eda_db_get(&db, 2, &record) && record.value == 10);
    assert(!mulib_eda_db_get(&db, 3, &record));
    mulib_eda_transaction_rollback(&t);
    mulib_eda_transaction_rollback(&t);
    assert(db.epoch == epoch && db.count == 2);

    assert(mulib_eda_transaction_begin(&t, &db));
    assert(mulib_eda_transaction_begin(&other, &db));
    assert(mulib_eda_db_put(&t.staged, (mulib_eda_db_record_t){1, 1, 7}));
    assert(mulib_eda_db_delete(&t.staged, 2));
    assert(mulib_eda_db_put(&t.staged, (mulib_eda_db_record_t){3, 3, 15}));
    assert(mulib_eda_transaction_commit(&t));
    assert(!t.active && !t.target && !t.staged.items);
    assert(db.epoch == epoch + 1 && db.count == 2);
    assert(mulib_eda_db_get(&db, 1, &record) && record.value == 7);
    assert(!mulib_eda_db_get(&db, 2, &record));
    assert(mulib_eda_db_get(&db, 3, &record) && record.value == 15);
    unsigned char before[sizeof other];
    memcpy(before, &other, sizeof other);
    assert(!mulib_eda_transaction_commit(&other));
    assert(!memcmp(before, &other, sizeof other));
    assert(mulib_eda_db_get(&other.staged, 1, &record) && record.value == 5);
    mulib_eda_transaction_rollback(&other);

    /* Preserve the existing external-write conflict regression. */
    assert(mulib_eda_transaction_begin(&t, &db));
    assert(mulib_eda_db_put(&db, (mulib_eda_db_record_t){1, 1, 9}));
    assert(!mulib_eda_transaction_commit(&t));
    mulib_eda_transaction_rollback(&t);
    assert(mulib_eda_db_get(&db, 1, &record) && record.value == 9);
    mulib_eda_db_free(&db);
}

static void empty_and_epoch_boundary(void) {
    mulib_eda_design_db_t db = {0};
    mulib_eda_transaction_t a = {0}, b = {0};
    assert(mulib_eda_transaction_begin(&a, &db));
    assert(mulib_eda_transaction_begin(&b, &db));
    assert(mulib_eda_transaction_commit(&a) && db.epoch == 1);
    assert(!mulib_eda_transaction_commit(&b) && b.active);
    mulib_eda_transaction_rollback(&b);
    db.epoch = UINT64_MAX - 1;
    assert(mulib_eda_transaction_begin(&a, &db));
    assert(mulib_eda_transaction_commit(&a) && db.epoch == UINT64_MAX);
    assert(mulib_eda_transaction_begin(&a, &db));
    assert(!mulib_eda_transaction_commit(&a) && a.active && db.epoch == UINT64_MAX);
    mulib_eda_transaction_rollback(&a);
    mulib_eda_db_free(&db);
}

int main(void) {
    invalid_ownership();
    multi_record_edits();
    empty_and_epoch_boundary();
    puts("PASS mulib-eda-transaction");
    return 0;
}
