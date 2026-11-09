/* Checked usage example for mulib-num-csc.h. */
#define MULIB_IMPL_NUM_SPARSE
#define MULIB_IMPL_NUM_CSC
#include "mulib-num-csc.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_num_sparse_t s = {0};
    mulib_num_csc_t c = {0};
    double x[] = {2, 4}, y[2];
    assert(mulib_num_sparse_init(&s, 2, 2));
    assert(mulib_num_sparse_add(&s, 0, 1, 3) && mulib_num_sparse_add(&s, 1, 0, 5));
    assert(mulib_num_csc_from_triplets(&s, &c) && c.offset[1] == 1);
    assert(mulib_num_csc_matvec(&c, x, y) && y[0] == 12 && y[1] == 10);
    mulib_num_csc_free(&c);
    mulib_num_sparse_free(&s);
    puts("PASS mulib-num-csc");
    return 0;
}
