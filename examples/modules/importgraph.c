#define MULIB_IMPL_IMPORTGRAPH
#include "mulib-importgraph.h"
#include <assert.h>
int main(void) {
    mulib_importgraph_t graph={0};graph.nodes=3;size_t order[3];
    assert(mulib_importgraph_add(&graph,2,0)&&mulib_importgraph_add(&graph,0,1));
    assert(mulib_importgraph_order(&graph,order,3)&&order[0]==2&&order[1]==0&&order[2]==1);
    mulib_importgraph_free(&graph);return 0;
}
