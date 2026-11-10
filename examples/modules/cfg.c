#define MULIB_IMPL_CFG
#include "mulib-cfg.h"
#include <assert.h>
int main(void) {
    mulib_cfg_t graph={0};graph.blocks=3;uint64_t reachable;size_t order[3],count;
    assert(mulib_cfg_add(&graph,0,1)&&mulib_cfg_add(&graph,1,2));
    assert(mulib_cfg_reachable(&graph,0,&reachable)&&reachable==7);
    assert(mulib_cfg_postorder(&graph,0,order,3,&count)&&count==3&&order[0]==2);
    mulib_cfg_free(&graph);return 0;
}
