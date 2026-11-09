/* Checked usage example for mulib-spice-dc.h. */
#define MULIB_IMPL_NUM_DENSE
#define MULIB_IMPL_SPICE_INDEPENDENT
#define MULIB_IMPL_SPICE_LEXER
#define MULIB_IMPL_SPICE_NETLIST
#define MULIB_IMPL_SPICE_OP
#define MULIB_IMPL_SPICE_R
#define MULIB_IMPL_SPICE_STAMP
#define MULIB_IMPL_SPICE_DC
#include "mulib-spice-dc.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_spice_netlist_t n = {0};
    double v[6] = {0};
    assert(mulib_spice_netlist_parse("V1 a 0 0\nR1 a 0 1k\n", &n, NULL));
    assert(mulib_spice_dc_sweep(&n, "V1", 0, 5, 3, v, 6));
    assert(v[0] == 0 && fabs(v[1] - 2.5) < 1e-12 && fabs(v[2] - 5) < 1e-12);
    assert(!mulib_spice_dc_sweep(&n, "notfound", 0, 5, 3, v, 6));
    puts("PASS mulib-spice-dc");
    return 0;
}
