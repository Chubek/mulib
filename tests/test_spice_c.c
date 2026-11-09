/* Focused executable regression for mulib-spice-c.h. */
#define MULIB_IMPL_SPICE_STAMP
#define MULIB_IMPL_SPICE_C
#include "mulib-spice-c.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_spice_stamp_t s={0};assert(mulib_spice_stamp_init(&s,1));assert(mulib_spice_c_backward_euler(&s,1,0,1e-6,1e-3,4,0));assert(fabs(s.a[0]-.001)<1e-12&&fabs(s.b[0]-.004)<1e-12);assert(!mulib_spice_c_backward_euler(&s,1,0,1e-6,0,4,0));mulib_spice_stamp_free(&s);
    puts("PASS mulib-spice-c");
    return 0;
}
