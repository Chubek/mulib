/* Checked usage example for mulib-spice-tran.h. */
#define MULIB_IMPL_NUM_DENSE
#define MULIB_IMPL_SPICE_C
#define MULIB_IMPL_SPICE_INDEPENDENT
#define MULIB_IMPL_SPICE_LEXER
#define MULIB_IMPL_SPICE_NETLIST
#define MULIB_IMPL_SPICE_R
#define MULIB_IMPL_SPICE_STAMP
#define MULIB_IMPL_SPICE_TRAN
#include "mulib-spice-tran.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_spice_netlist_t n = {0};
    double x[6] = {0};
    assert(mulib_spice_netlist_parse("V1 in 0 1\nR1 in out 1k\nC1 out 0 1u\n", &n, NULL));
    int out = mulib_spice_netlist_find_node(&n, "out");
    assert(mulib_spice_tran_fixed(&n, .001, 6, (uint16_t)out, x));
    assert(fabs(x[0] - .5) < 1e-9 && fabs(x[1] - .75) < 1e-9 && x[5] > .98);
    assert(!mulib_spice_tran_fixed(&n, 0, 6, (uint16_t)out, x));
    puts("PASS mulib-spice-tran");
    return 0;
}
