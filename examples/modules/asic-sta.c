/* Checked usage example for mulib-asic-sta.h. */
#define MULIB_IMPL_ASIC_STA
#include "mulib-asic-sta.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_asic_timing_node_t n[3] = {0};
    double cp = 0;
    n[0].delay = 1;
    n[1].delay = 2;
    n[1].pred[0] = 0;
    n[1].pred_count = 1;
    n[2].delay = 3;
    n[2].pred[0] = 1;
    n[2].pred_count = 1;
    assert(mulib_asic_sta_propagate(n, 3, &cp) && fabs(cp - 6) < 1e-12);
    n[2].pred[0] = 2;
    assert(!mulib_asic_sta_propagate(n, 3, &cp));
    puts("PASS mulib-asic-sta");
    return 0;
}
