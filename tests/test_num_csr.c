/* Canonical CSR, bounded view bridge, and independent matvec/solve oracles. */
#define MULIB_IMPL_NUM_SPARSE
#define MULIB_IMPL_NUM_SPARSE_LU
#include "mulib-num-sparse.h"
#include "mulib-num-sparse-lu.h"
#include <assert.h>

/* Inject and count allocations in the canonical CSR implementation only. */
static size_t budget = SIZE_MAX, live;

static void *csr_malloc(size_t n) {
    if (!budget)
        return NULL;
    if (budget != SIZE_MAX)
        budget--;
    void *p = malloc(n);
    if (p)
        live++;
    return p;
}

static void *csr_calloc(size_t n, size_t k) {
    assert(k && n <= SIZE_MAX / k);
    void *p = csr_malloc(n * k);
    if (p)
        memset(p, 0, n * k);
    return p;
}

static void csr_free(void *p) {
    if (p) {
        assert(live);
        live--;
    }
    free(p);
}

#define malloc csr_malloc
#define calloc csr_calloc
#define free csr_free
#define MULIB_IMPL_NUM_CSR
#include "mulib-num-csr.h"
#undef malloc
#undef calloc
#undef free

static void triplets(void) {
    mulib_num_sparse_t s = {0};
    mulib_num_csr_t c = {0};
    double x[] = {1, 2};
    assert(mulib_num_sparse_init(&s, 2, 2));
    assert(mulib_num_sparse_add(&s, 1, 1, 4) && mulib_num_sparse_add(&s, 0, 0, 5));
    assert(mulib_num_csr_from_triplets(&s, &c));
    assert(c.offset[0] == 0 && c.offset[1] == 1 && c.offset[2] == 2);
    assert(mulib_num_csr_matvec(&c, x, x) && x[0] == 5 && x[1] == 8);
    mulib_num_csr_free(&c);
    mulib_num_sparse_free(&s);
}

static void round_trip(void) {
    /* Effective matrix:
     *    0   2   1
     *    3  -1   2
     *    1   1   0
     * It needs a row pivot and is intentionally nonsymmetric. */
    size_t offset[] = {0, 4, 7, 9};
    size_t column[] = {2, 1, 1, 0, 2, 0, 1, 1, 0};
    double values[] = {1, .75, 1.25, -0.0, 2, 3, -1, 1, 1};
    mulib_num_csr_view src = {3, 3, 9, offset, column, values};
    mulib_num_csr_t owner = {0}, t = {0};
    assert(mulib_num_csr_copy_from_view(&src, 4, 9, 9, &owner));
    assert(owner.offset != offset && owner.column != column && owner.values != values);
    assert(!memcmp(owner.offset, offset, sizeof offset));
    assert(!memcmp(owner.column, column, sizeof column));
    assert(!memcmp(owner.values, values, sizeof values));
    offset[1] = 0;
    column[0] = 0;
    values[0] = 100;
    assert(mulib_num_csr_validate(&owner));

    mulib_num_csr_view view = {0};
    budget = 0; /* It must not allocate. */
    assert(mulib_num_csr_borrow_view(&owner, 4, 9, 9, &view));
    budget = SIZE_MAX;
    assert(view.rows == 3 && view.cols == 3 && view.nnz == 9);
    assert(view.ptr == owner.offset && view.idx == owner.column && view.val == owner.values);

    const double expected_x[] = {1, -2, 3}, rhs[] = {-1, 11, -1};
    double y[3], x[3];
    assert(mulib_num_csr_matvec(&owner, expected_x, y));
    for (size_t i = 0; i < 3; i++)
        assert(y[i] == rhs[i]);

    assert(mulib_num_csr_transpose(&owner, &t));
    assert(t.rows == 3 && t.cols == 3 && t.nnz == 9);
    assert(mulib_num_csr_matvec(&t, expected_x, y));
    assert(y[0] == -3 && y[1] == 7 && y[2] == -3);
    mulib_num_csr_free(&t);

    mulib_num_sparse_lu lu = {0};
    assert(mulib_num_sparse_lu_factor(&view, 1e-12, &lu));
    assert(lu.piv[0] != 0);
    mulib_num_csr_free(&owner); /* The factor, unlike the view, owns its data. */
    assert(mulib_num_sparse_lu_solve(&lu, rhs, x));
    mulib_num_sparse_lu_free(&lu);
    for (size_t i = 0; i < 3; i++)
        assert(fabs(x[i] - expected_x[i]) < 1e-12);
    double r0 = 2*x[1] + x[2] - rhs[0];
    double r1 = 3*x[0] - x[1] + 2*x[2] - rhs[1];
    double r2 = x[0] + x[1] - rhs[2];
    assert(hypot(hypot(r0, r1), r2) < 1e-12);
}

static void rejects_without_publication(void) {
    size_t offset[] = {0, 0, 1}, column[] = {1};
    double values[] = {DBL_MAX};
    const mulib_num_csr_view src = {2, 2, 1, offset, column, values};
    mulib_num_csr_t dst = {0};
    unsigned char saved[sizeof dst];
    memcpy(saved, &dst, sizeof saved);
    assert(!mulib_num_csr_copy_from_view(NULL, 3, 1, 1, &dst));
    assert(!mulib_num_csr_copy_from_view(&src, 3, 1, 1, NULL));
    assert(!mulib_num_csr_copy_from_view(&src, 2, 1, 1, &dst));
    assert(!mulib_num_csr_copy_from_view(&src, 3, 0, 1, &dst));
    assert(!mulib_num_csr_copy_from_view(&src, 3, 1, 0, &dst));
    offset[0] = 1;
    assert(!mulib_num_csr_copy_from_view(&src, 3, 1, 1, &dst));
    offset[0] = 0;
    offset[1] = 2;
    assert(!mulib_num_csr_copy_from_view(&src, 3, 1, 1, &dst));
    offset[1] = 0;
    offset[2] = 0;
    assert(!mulib_num_csr_copy_from_view(&src, 3, 1, 1, &dst));
    offset[2] = 1;
    column[0] = 2;
    assert(!mulib_num_csr_copy_from_view(&src, 3, 1, 1, &dst));
    column[0] = 1;
    values[0] = NAN;
    assert(!mulib_num_csr_copy_from_view(&src, 3, 1, 1, &dst));
    values[0] = INFINITY;
    assert(!mulib_num_csr_copy_from_view(&src, 3, 1, 1, &dst));
    values[0] = DBL_MAX;
    mulib_num_csr_view no_values = src;
    no_values.val = NULL;
    assert(!mulib_num_csr_copy_from_view(&no_values, 3, 1, 1, &dst));
    mulib_num_csr_view huge = src;
    huge.rows = SIZE_MAX;
    assert(!mulib_num_csr_view_valid(&huge));
    assert(!mulib_num_csr_copy_from_view(&huge, 3, 1, 1, &dst));
    huge = src;
    huge.nnz = SIZE_MAX;
    assert(!mulib_num_csr_view_valid(&huge));
    assert(!mulib_num_csr_copy_from_view(&huge, 3, 1, 1, &dst));
    assert(!memcmp(saved, &dst, sizeof dst) && !live);

    for (size_t i = 0; i < 3; i++) {
        budget = i;
        assert(!mulib_num_csr_copy_from_view(&src, 3, 1, 1, &dst));
        assert(!memcmp(saved, &dst, sizeof dst) && !live);
    }
    budget = SIZE_MAX;
    assert(mulib_num_csr_copy_from_view(&src, 3, 1, 1, &dst));
    memcpy(saved, &dst, sizeof saved);
    assert(!mulib_num_csr_copy_from_view(&src, 3, 1, 1, &dst));
    assert(!memcmp(saved, &dst, sizeof dst) && live == 3);

    mulib_num_csr_view view = src;
    unsigned char saved_view[sizeof view];
    memcpy(saved_view, &view, sizeof saved_view);
    assert(!mulib_num_csr_borrow_view(&dst, 0, 1, 1, &view));
    assert(!mulib_num_csr_borrow_view(&dst, 3, 0, 1, &view));
    assert(!mulib_num_csr_borrow_view(&dst, 3, 1, 0, &view));
    dst.column[0] = dst.cols;
    assert(!mulib_num_csr_borrow_view(&dst, 3, 1, 1, &view));
    assert(!mulib_num_csr_validate(&dst));
    assert(!memcmp(saved_view, &view, sizeof view));
    dst.column[0] = 1;
    const double x[] = {0, 2};
    double y[] = {123, 456};
    assert(!mulib_num_csr_matvec(&dst, x, y) && y[0] == 123 && y[1] == 456);
    mulib_num_csr_free(&dst);
}

static void empty_rectangular(void) {
    const size_t zero = 0;
    const mulib_num_csr_view empty = {0, 3, 0, &zero, NULL, NULL};
    mulib_num_csr_t owner = {0}, t = {0};
    assert(!mulib_num_csr_copy_from_view(&empty, 0, 0, 0, &owner));
    assert(!mulib_num_csr_copy_from_view(&empty, 1, 1, 0, &owner));
    assert(!mulib_num_csr_copy_from_view(&empty, 1, 0, 1, &owner));
    assert(mulib_num_csr_copy_from_view(&empty, 1, 0, 0, &owner));
    assert(owner.offset[0] == 0 && !owner.column && !owner.values);
    assert(mulib_num_csr_transpose(&owner, &t));
    assert(t.rows == 3 && !t.cols && !t.nnz);
    mulib_num_csr_view view = {0};
    assert(mulib_num_csr_borrow_view(&t, 4, 0, 0, &view));
    for (size_t i = 0; i < 4; i++)
        assert(view.ptr[i] == 0);
    mulib_num_csr_free(&t);
    mulib_num_csr_free(&owner);
}

int main(void) {
    triplets();
    round_trip();
    rejects_without_publication();
    empty_rectangular();
    assert(!live);
    puts("PASS mulib-num-csr");
    return 0;
}
