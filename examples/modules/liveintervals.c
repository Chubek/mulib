#define MULIB_IMPL_LIVEINTERVALS
#include "mulib-liveintervals.h"
#include <assert.h>
int main(void) {
    mulib_liveinterval_t a={0,1,3},b={1,3,5},c={2,4,6};
    assert(mulib_liveinterval_valid(&a)&&mulib_liveinterval_overlap(&a,&b));
    assert(!mulib_liveinterval_overlap(&a,&c));return 0;
}
