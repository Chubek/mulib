/* Checked usage example for mulib-eda-netgraph.h. */
#define MULIB_IMPL_EDA_NETGRAPH
#include "mulib-eda-netgraph.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_eda_netgraph_t g = {0};
    uint32_t n, t1, t2;
    assert(mulib_eda_netgraph_net(&g, &n));
    assert(mulib_eda_netgraph_terminal(&g, 1, &t1));
    assert(mulib_eda_netgraph_terminal(&g, 2, &t2));
    assert(mulib_eda_netgraph_connect(&g, t1, n) && mulib_eda_netgraph_connect(&g, t2, n));
    assert(mulib_eda_netgraph_degree(&g, n) == 2);
    assert(!mulib_eda_netgraph_connect(&g, t1, 42));
    mulib_eda_netgraph_free(&g);
    puts("PASS mulib-eda-netgraph");
    return 0;
}
