/* Checked usage example for mulib-flow-graph.h. */
#define MULIB_IMPL_FLOW_GRAPH
#include "mulib-flow-graph.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_flow_graph_t g = {0};
    uint8_t seq[64];
    assert(mulib_flow_graph_add(&g, 3));
    assert(mulib_flow_graph_depend(&g, 0, 1) && mulib_flow_graph_depend(&g, 1, 2));
    assert(mulib_flow_graph_toposort(&g, seq) && seq[0] == 0 && seq[1] == 1 && seq[2] == 2);
    assert(mulib_flow_graph_depend(&g, 2, 0));
    assert(!mulib_flow_graph_toposort(&g, seq));
    puts("PASS mulib-flow-graph");
    return 0;
}
