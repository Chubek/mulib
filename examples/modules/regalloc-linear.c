#define MULIB_IMPL_LIVEINTERVALS
#define MULIB_IMPL_REGALLOC_LINEAR
#include "mulib-regalloc-linear.h"
#include <assert.h>
int main(void) {
    mulib_liveinterval_t ranges[]={{0,0,10},{1,1,2},{2,3,4}};
    mulib_regalloc_location_t locations[3];size_t spills;
    assert(mulib_regalloc_linear(ranges,3,1,locations,&spills)&&spills==1);
    assert(locations[0].reg==-1&&locations[1].reg==0&&locations[2].reg==0);return 0;
}
