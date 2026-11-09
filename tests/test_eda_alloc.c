/* Inject failure at each allocation in multi-allocation EDA operations. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
static size_t remaining = SIZE_MAX, live;
static int allocation_allowed(void) {
    if (remaining == SIZE_MAX)
        return 1;
    if (!remaining)
        return 0;
    remaining--;
    return 1;
}
static void *checked_malloc(size_t n) {
    if (!allocation_allowed())
        return NULL;
    void *p = malloc(n);
    if (p)
        live++;
    return p;
}
static void *checked_calloc(size_t n, size_t k) {
    if (!allocation_allowed())
        return NULL;
    void *p = calloc(n, k);
    if (p)
        live++;
    return p;
}
static void *checked_realloc(void *p, size_t n) {
    if (!allocation_allowed())
        return NULL;
    int empty = p == NULL;
    void *r = realloc(p, n);
    if (r && empty)
        live++;
    return r;
}
static void checked_free(void *p) {
    if (p) {
        assert(live);
        live--;
    }
    free(p);
}
#define malloc checked_malloc
#define calloc checked_calloc
#define realloc checked_realloc
#define free checked_free
#include "amb3r_implementation.c"
#undef malloc
#undef calloc
#undef realloc
#undef free

int main(void) {
    for (size_t fail = 0; fail < 5; fail++) {
        remaining = fail;
        mulib_eda_id_pool_t ids = {0};
        int ok = mulib_eda_id_pool_init(&ids, 4);
        assert(ok == (fail >= 3));
        mulib_eda_id_pool_free(&ids);
        assert(!live);
        remaining = fail;
        mulib_spice_stamp_t stamp = {0};
        ok = mulib_spice_stamp_init(&stamp, 2);
        assert(ok == (fail >= 2));
        mulib_spice_stamp_free(&stamp);
        assert(!live);
        remaining = fail;
        mulib_eda_incremental_t graph = {0};
        ok = mulib_eda_incremental_init(&graph, 3);
        assert(ok == (fail >= 2));
        mulib_eda_incremental_free(&graph);
        assert(!live);
    }
    for (size_t fail = 0; fail < 6; fail++) {
        remaining = SIZE_MAX;
        mulib_num_sparse_t sparse = {0};
        assert(mulib_num_sparse_init(&sparse, 2, 2));
        assert(mulib_num_sparse_add(&sparse, 0, 0, 3));
        size_t baseline = live;
        mulib_num_csr_t csr = {0};
        mulib_num_csc_t csc = {0};
        remaining = fail;
        int ok = mulib_num_csr_from_triplets(&sparse, &csr);
        assert(ok == (fail >= 4));
        mulib_num_csr_free(&csr);
        assert(live == baseline);
        remaining = fail;
        ok = mulib_num_csc_from_triplets(&sparse, &csc);
        assert(ok == (fail >= 4));
        mulib_num_csc_free(&csc);
        assert(live == baseline);
        remaining = SIZE_MAX;
        assert(mulib_num_csr_from_triplets(&sparse, &csr));
        baseline = live;
        mulib_num_csr_t transposed = {0};
        remaining = fail;
        ok = mulib_num_csr_transpose(&csr, &transposed);
        assert(ok == (fail >= 4));
        mulib_num_csr_free(&transposed);
        assert(live == baseline);
        double input[] = {2, 1}, output[] = {42, 43};
        remaining = 0;
        assert(!mulib_num_csr_matvec(&csr, input, output) && output[0] == 42 && output[1] == 43);
        assert(!mulib_num_sparse_canonicalize(&sparse) && sparse.count == 1 &&
               sparse.triplets[0].value == 3);
        mulib_num_csr_free(&csr);
        mulib_num_sparse_free(&sparse);
        assert(!live);
    }
    remaining = SIZE_MAX;
    mulib_eda_design_db_t db = {0}, clone = {0};
    assert(mulib_eda_db_put(&db, (mulib_eda_db_record_t){1, 1, 2}));
    size_t baseline = live;
    remaining = 0;
    assert(!mulib_eda_db_clone(&db, &clone) && !clone.items);
    mulib_eda_transaction_t transaction = {0};
    assert(!mulib_eda_transaction_begin(&transaction, &db) && !transaction.active);
    mulib_eda_transaction_rollback(&transaction);
    assert(live == baseline);
    mulib_eda_db_free(&db);
    assert(!live);
    puts("PASS EDA allocation failures");
    return 0;
}
