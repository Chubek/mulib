/* Checked usage example for mulib-num-dense.h. */
#define MULIB_IMPL_NUM_DENSE
#include "mulib-num-dense.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    double a[] = {3, 1, 1, 2}, b[] = {9, 8}, x[] = {99, 99};
    assert(mulib_num_dense_solve(2, a, b, x, 1e-12));
    assert(fabs(x[0] - 2) < 1e-12 && fabs(x[1] - 3) < 1e-12);
    double s[] = {1, 1, 2, 2};
    assert(!mulib_num_dense_solve(2, s, b, x, 1e-12));
    assert(fabs(x[0] - 2) < 1e-12);
    mulib_num_dense_t m = {0};
    assert(mulib_num_dense_init(&m, 2, 3) && m.a[0] == 0);
    mulib_num_dense_free(&m);
    puts("PASS mulib-num-dense");
    return 0;
}
