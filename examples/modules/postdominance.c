#define MULIB_IMPL_CFG
#define MULIB_IMPL_DOMINANCE
#define MULIB_IMPL_POSTDOMINANCE
#include "mulib-postdominance.h"
#include <assert.h>
int main(void) {
    mulib_cfg_t graph={0};graph.blocks=3;mulib_dominance_t postdom;
    assert(mulib_cfg_add(&graph,0,1)&&mulib_cfg_add(&graph,1,2));
    assert(mulib_postdominance_compute(&graph,2,&postdom)&&postdom.idom[0]==1);
    assert(mulib_dominance_test(&postdom,2,0));mulib_cfg_free(&graph);return 0;
}
