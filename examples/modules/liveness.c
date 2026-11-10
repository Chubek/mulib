#define MULIB_IMPL_CFG
#define MULIB_IMPL_LIVENESS
#include "mulib-liveness.h"
#include <assert.h>
int main(void) {
    mulib_cfg_t graph={0};graph.blocks=2;mulib_liveness_t live;
    uint64_t use[]={0,1},def[]={1,0};assert(mulib_cfg_add(&graph,0,1));
    assert(mulib_liveness_compute(&graph,use,def,&live)&&live.out[0]==1&&live.in[0]==0);
    mulib_cfg_free(&graph);return 0;
}
