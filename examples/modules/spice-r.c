/* Checked usage example for mulib-spice-r.h. */
#define MULIB_IMPL_SPICE_STAMP
#define MULIB_IMPL_SPICE_R
#include "mulib-spice-r.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_spice_stamp_t s = {0};
    assert(mulib_spice_stamp_init(&s, 1));
    assert(mulib_spice_r_stamp(&s, 1, 0, 1000));
    assert(fabs(s.a[0] - .001) < 1e-12);
    assert(!mulib_spice_r_stamp(&s, 1, 0, -1));
    mulib_spice_stamp_free(&s);
    puts("PASS mulib-spice-r");
    return 0;
}
