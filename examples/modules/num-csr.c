/* Checked usage example for mulib-num-csr.h. */
#define MULIB_IMPL_NUM_SPARSE
#define MULIB_IMPL_NUM_CSR
#include "mulib-num-csr.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_num_sparse_t s = {0};
    mulib_num_csr_t c = {0};
    double x[] = {1, 2}, y[] = {0, 0};
    assert(mulib_num_sparse_init(&s, 2, 2));
    assert(mulib_num_sparse_add(&s, 1, 1, 4) && mulib_num_sparse_add(&s, 0, 0, 5));
    assert(mulib_num_csr_from_triplets(&s, &c));
    assert(c.offset[0] == 0 && c.offset[1] == 1 && c.offset[2] == 2);
    assert(mulib_num_csr_matvec(&c, x, y) && y[0] == 5 && y[1] == 8);
    mulib_num_csr_free(&c);
    mulib_num_sparse_free(&s);
    puts("PASS mulib-num-csr");
    return 0;
}
