#define MULIB_IMPL_REGALLOC_GRAPH
#include "mulib-regalloc-graph.h"
#include <assert.h>
int main(void) {
    uint64_t interference[]={6,5,3};int colors[3];size_t spills;
    assert(mulib_regalloc_graph(interference,3,3,colors,&spills)&&spills==1);
    assert(colors[0]==0&&colors[1]==1&&colors[2]==-1);return 0;
}
