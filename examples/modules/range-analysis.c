#define MULIB_IMPL_RANGE_ANALYSIS
#include "mulib-range-analysis.h"
#include <assert.h>
int main(void) {
    mulib_range_t a=mulib_range_join(mulib_range_const(-3),mulib_range_const(2));
    mulib_range_t product=mulib_range_mul(a,mulib_range_const(7));
    assert(product.known&&product.lo==-21&&product.hi==14);
    assert(!mulib_range_add(mulib_range_const(INT64_MAX),mulib_range_const(1)).known);
    return 0;
}
