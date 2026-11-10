/* Checked usage example for mulib-eda-ids.h. */
#define MULIB_IMPL_EDA_IDS
#define MULIB_IMPL_EDA_DESIGN_DB
#define MULIB_IMPL_EDA_TRANSACTION
#include "mulib-eda-ids.h"
#include "mulib-eda-transaction.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_eda_id_pool_t p;
    mulib_eda_design_db_t db = {0};
    mulib_eda_transaction_t edit = {0};
    mulib_eda_db_record_t record;
    mulib_eda_id_t a, b;
    assert(mulib_eda_id_pool_init(&p, 1));
    assert(mulib_eda_id_create(&p, 7, &a));
    assert(mulib_eda_id_valid(&p, a, 7));
    assert(!mulib_eda_id_valid(&p, a, 2));
    /* Scalar design-record adapter: explicitly validate pool membership. */
    assert(mulib_eda_db_put(&db, (mulib_eda_db_record_t){a, 7, 1000}));
    assert(mulib_eda_transaction_begin(&edit, &db));
    assert(mulib_eda_db_put(&edit.staged, (mulib_eda_db_record_t){a, 7, 2200}));
    mulib_eda_transaction_rollback(&edit);
    assert(mulib_eda_db_get(&db, a, &record) && record.value == 1000);
    assert(mulib_eda_transaction_begin(&edit, &db));
    assert(mulib_eda_db_put(&edit.staged, (mulib_eda_db_record_t){a, 7, 2200}));
    assert(mulib_eda_transaction_commit(&edit));
    assert(mulib_eda_db_get(&db, a, &record) && record.value == 2200);
    /* Caller-serialized deletion: remove the record before retiring its ID. */
    assert(mulib_eda_transaction_begin(&edit, &db));
    assert(mulib_eda_db_delete(&edit.staged, a));
    assert(mulib_eda_transaction_commit(&edit));
    assert(mulib_eda_id_remove(&p, a));
    assert(!mulib_eda_id_valid(&p, a, 7));
    assert(mulib_eda_id_create(&p, 7, &b));
    assert(mulib_eda_id_index(a) == mulib_eda_id_index(b) && a != b);
    assert(!mulib_eda_id_remove(&p, a));
    assert(mulib_eda_db_put(&db, (mulib_eda_db_record_t){b, 7, 3300}));
    assert(!mulib_eda_db_get(&db, a, &record));
    assert(mulib_eda_id_valid(&p, b, 7));
    assert(mulib_eda_db_get(&db, b, &record) && record.value == 3300);
    mulib_eda_db_free(&db);
    mulib_eda_id_pool_free(&p);
    puts("PASS mulib-eda-ids");
    return 0;
}
