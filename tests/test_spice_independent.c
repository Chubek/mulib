/* Focused executable regression for mulib-spice-independent.h. */
#define MULIB_IMPL_SPICE_STAMP
#define MULIB_IMPL_SPICE_INDEPENDENT
#include "mulib-spice-independent.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_spice_stamp_t s={0};assert(mulib_spice_stamp_init(&s,2));assert(mulib_spice_independent_voltage(&s,1,0,1,5));assert(s.b[1]==5&&s.a[1]==1&&s.a[2]==1);assert(mulib_spice_independent_current(&s,1,0,2)&&s.b[0]==-2);mulib_spice_stamp_free(&s);
    puts("PASS mulib-spice-independent");
    return 0;
}
