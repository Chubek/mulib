/* Checked usage example for mulib-eda-incremental.h. */
#define MULIB_IMPL_EDA_INCREMENTAL
#include "mulib-eda-incremental.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_eda_incremental_t g = {0};
    assert(mulib_eda_incremental_init(&g, 4));
    assert(mulib_eda_incremental_depend(&g, 0, 1) && mulib_eda_incremental_depend(&g, 1, 2));
    assert(mulib_eda_incremental_invalidate(&g, 0));
    assert(g.dirty[0] && g.dirty[1] && g.dirty[2] && !g.dirty[3]);
    mulib_eda_incremental_clear(&g);
    assert(!g.dirty[1]);
    mulib_eda_incremental_free(&g);
    puts("PASS mulib-eda-incremental");
    return 0;
}
