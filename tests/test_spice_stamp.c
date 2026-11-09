/* Focused executable regression for mulib-spice-stamp.h. */
#define MULIB_IMPL_SPICE_STAMP
#include "mulib-spice-stamp.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_spice_stamp_t s={0};assert(mulib_spice_stamp_init(&s,2));assert(mulib_spice_stamp_conductance(&s,1,0,0.001));assert(mulib_spice_stamp_current(&s,1,0,0.002));assert(fabs(s.a[0]-.001)<1e-12&&fabs(s.b[0]+.002)<1e-12);assert(!mulib_spice_stamp_current(&s,3,0,1));mulib_spice_stamp_free(&s);
    puts("PASS mulib-spice-stamp");
    return 0;
}
