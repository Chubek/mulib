/* Checked usage example for mulib-eda-transaction.h. */
#define MULIB_IMPL_EDA_DESIGN_DB
#define MULIB_IMPL_EDA_TRANSACTION
#include "mulib-eda-transaction.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_eda_design_db_t db = {0};
    mulib_eda_transaction_t t = {0}, t2 = {0};
    mulib_eda_db_record_t a = {1, 1, 5};
    assert(mulib_eda_db_put(&db, a));
    assert(mulib_eda_db_put(&db, (mulib_eda_db_record_t){2, 2, 10}));
    uint64_t epoch = db.epoch;
    assert(mulib_eda_transaction_begin(&t, &db));
    a.value = 7;
    assert(mulib_eda_db_put(&t.staged, a));
    assert(mulib_eda_db_delete(&t.staged, 2));
    assert(mulib_eda_db_put(&t.staged, (mulib_eda_db_record_t){3, 2, 20}));
    assert(db.epoch == epoch && mulib_eda_db_get(&db, 2, &a));
    assert(mulib_eda_transaction_commit(&t));
    assert(db.epoch == epoch + 1);
    assert(mulib_eda_db_get(&db, 1, &a) && a.value == 7);
    assert(!mulib_eda_db_get(&db, 2, &a));
    assert(mulib_eda_db_get(&db, 3, &a) && a.value == 20);
    assert(mulib_eda_transaction_begin(&t2, &db));
    assert(mulib_eda_db_get(&db, 1, &a));
    a.value = 9;
    assert(mulib_eda_db_put(&db, a));
    assert(!mulib_eda_transaction_commit(&t2));
    assert(mulib_eda_db_get(&t2.staged, 1, &a) && a.value == 7);
    mulib_eda_transaction_rollback(&t2);
    assert(mulib_eda_db_get(&db, 1, &a) && a.value == 9);
    mulib_eda_db_free(&db);
    puts("PASS mulib-eda-transaction");
    return 0;
}
