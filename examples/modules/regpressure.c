#define MULIB_IMPL_LIVEINTERVALS
#define MULIB_IMPL_REGPRESSURE
#include "mulib-regpressure.h"
#include <assert.h>
int main(void) {
    mulib_liveinterval_t ranges[]={{0,0,2},{1,2,4},{2,5,6}};size_t peak;
    assert(mulib_regpressure_peak(ranges,3,&peak)&&peak==2);return 0;
}
