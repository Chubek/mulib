/* Focused executable regression for mulib-num-sparse.h. */
#define MULIB_IMPL_NUM_SPARSE
#include "mulib-num-sparse.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_num_sparse_t s={0};double x[]={1,2},y[]={0,0};assert(mulib_num_sparse_init(&s,2,2));assert(mulib_num_sparse_add(&s,0,0,2)&&mulib_num_sparse_add(&s,0,0,3)&&mulib_num_sparse_add(&s,1,1,4));assert(mulib_num_sparse_matvec(&s,x,y));assert(y[0]==5&&y[1]==8);assert(!mulib_num_sparse_add(&s,4,0,1));mulib_num_sparse_free(&s);
    puts("PASS mulib-num-sparse");
    return 0;
}
