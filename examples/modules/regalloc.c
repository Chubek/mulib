#define MULIB_IMPL_LIVEINTERVALS
#define MULIB_IMPL_REGALLOC_LINEAR
#define MULIB_IMPL_REGALLOC_GRAPH
#define MULIB_IMPL_REGALLOC
#include "mulib-regalloc.h"
#include <assert.h>
int main(void) {
    uint64_t interference[]={2,1};mulib_regalloc_location_t locations[2];size_t spills;
    assert(mulib_regalloc_allocate(MULIB_REGALLOC_GRAPH,NULL,2,interference,3,locations,&spills));
    assert(!spills&&locations[0].reg!=locations[1].reg);return 0;
}
