/* Link against a C11-compiled MuLIB implementation object from C++17. */
#include "mulib-eda-status.h"
#include "mulib-eda-ids.h"
#include "mulib-eda-transaction.h"
#include "mulib-dsim-logic4.h"
#include "mulib-rf-sparams.h"
#include "mulib-hwil-module.h"
#include "mulib-dsim-bitvec.h"
#include "mulib-num-sparse.h"
#include "mulib-num-csr.h"
#include <cassert>
#include <cmath>
int main() {
    mulib_eda_id_pool_t pool{};
    mulib_eda_id_t id = 0;
    assert(mulib_eda_id_pool_init(&pool, 4));
    assert(mulib_eda_id_create(&pool, 7, &id));
    assert(mulib_eda_id_valid(&pool, id, 7));
    pool.generation[0] = 0xfffffeu;
    id = (UINT64_C(7) << 56) | (UINT64_C(0xfffffe) << 32) | 1;
    assert(mulib_eda_id_remove(&pool, id));
    mulib_eda_id_t last = 0, next = 0;
    assert(mulib_eda_id_create(&pool, 7, &last));
    assert(mulib_eda_id_index(last) == 0);
    assert(((last >> 32) & 0xffffffu) == 0xffffffu);
    assert(!mulib_eda_id_valid(&pool, id, 7));
    assert(mulib_eda_id_remove(&pool, last));
    assert(mulib_eda_id_create(&pool, 7, &next));
    assert(mulib_eda_id_index(next) == 1);
    assert(!mulib_eda_id_valid(&pool, last, 7));
    mulib_eda_id_pool_free(&pool);
    mulib_eda_design_db_t db{};
    mulib_eda_transaction_t edit{};
    mulib_eda_db_record_t record{1, 1, 1000};
    assert(!mulib_eda_transaction_begin(&edit, &edit.staged));
    assert(mulib_eda_db_put(&db, record));
    assert(mulib_eda_transaction_begin(&edit, &db));
    record.value = 2200;
    assert(mulib_eda_db_put(&edit.staged, record));
    mulib_eda_transaction_rollback(&edit);
    assert(mulib_eda_db_get(&db, 1, &record) && record.value == 1000);
    assert(mulib_eda_transaction_begin(&edit, &db));
    record.value = 3300;
    assert(mulib_eda_db_put(&edit.staged, record));
    assert(mulib_eda_transaction_commit(&edit));
    assert(mulib_eda_db_get(&db, 1, &record) && record.value == 3300);
    mulib_eda_db_free(&db);
    assert(mulib_dsim_and(MULIB_DSIM_0, MULIB_DSIM_X) == MULIB_DSIM_0);
    mulib_rf_complex_t z{50, 0}, gamma{};
    assert(mulib_rf_s11_from_impedance(z, 50, &gamma));
    assert(std::abs(gamma.re) < 1e-12);
    mulib_dsim_bitvec_t a{}, b{}, sum{};
    assert(mulib_dsim_bitvec_parse(&a, "0011"));
    assert(mulib_dsim_bitvec_parse(&b, "0101"));
    assert(mulib_dsim_bitvec_add(a, b, &sum) && sum.value == 8);
    mulib_num_sparse_t sparse{};
    mulib_num_csr_t csr{}, transpose{};
    assert(mulib_num_sparse_init(&sparse, 1, 2));
    assert(mulib_num_sparse_add(&sparse, 0, 1, 3));
    assert(mulib_num_csr_from_triplets(&sparse, &csr));
    assert(mulib_num_csr_transpose(&csr, &transpose));
    assert(transpose.rows == 2 && transpose.cols == 1 && transpose.values[0] == 3);
    mulib_num_csr_free(&transpose);
    mulib_num_csr_free(&csr);
    mulib_num_sparse_free(&sparse);
    return 0;
}
