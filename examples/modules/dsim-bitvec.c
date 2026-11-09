/* Checked usage example for mulib-dsim-bitvec.h. */
#define MULIB_IMPL_DSIM_LOGIC4
#define MULIB_IMPL_DSIM_BITVEC
#include "mulib-dsim-bitvec.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_dsim_bitvec_t a, b, c;
    char text[65];
    assert(mulib_dsim_bitvec_parse(&a, "10xz") && mulib_dsim_bitvec_format(a, text, sizeof text) &&
           !strcmp(text, "10xz"));
    assert(mulib_dsim_bitvec_parse(&b, "0111") && mulib_dsim_bitvec_and(a, b, &c));
    assert(mulib_dsim_bitvec_format(c, text, sizeof text) && !strcmp(text, "00xx"));
    assert(!mulib_dsim_bitvec_parse(&b, "10?"));
    puts("PASS mulib-dsim-bitvec");
    return 0;
}
