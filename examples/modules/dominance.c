#define MULIB_IMPL_CFG
#define MULIB_IMPL_DOMINANCE
#include "mulib-dominance.h"
#include <assert.h>
int main(void) {
    mulib_cfg_t graph={0};graph.blocks=3;mulib_dominance_t dom;
    assert(mulib_cfg_add(&graph,0,1)&&mulib_cfg_add(&graph,1,2));
    assert(mulib_dominance_compute(&graph,0,&dom)&&dom.idom[2]==1);
    assert(mulib_dominance_test(&dom,0,2));mulib_cfg_free(&graph);return 0;
}
