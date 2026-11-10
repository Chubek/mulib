#define MULIB_IMPL_CFG
#define MULIB_IMPL_DOMINANCE
#define MULIB_IMPL_LOOPS
#include "mulib-loops.h"
#include <assert.h>
int main(void) {
    mulib_cfg_t graph={0};graph.blocks=3;mulib_dominance_t dom;mulib_loops_t loops;
    assert(mulib_cfg_add(&graph,0,1)&&mulib_cfg_add(&graph,1,2)&&mulib_cfg_add(&graph,2,1));
    assert(mulib_dominance_compute(&graph,0,&dom));
    assert(mulib_loops_find(&graph,&dom,&loops)&&loops.count==1&&loops.loops[0].members==6);
    mulib_cfg_free(&graph);return 0;
}
