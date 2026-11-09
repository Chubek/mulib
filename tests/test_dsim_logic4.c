/* Focused executable regression for mulib-dsim-logic4.h. */
#define MULIB_IMPL_DSIM_LOGIC4
#include "mulib-dsim-logic4.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
assert(mulib_dsim_and(MULIB_DSIM_0,MULIB_DSIM_X)==MULIB_DSIM_0);assert(mulib_dsim_and(MULIB_DSIM_1,MULIB_DSIM_Z)==MULIB_DSIM_X);assert(mulib_dsim_or(MULIB_DSIM_1,MULIB_DSIM_X)==MULIB_DSIM_1);assert(mulib_dsim_resolve(MULIB_DSIM_1,MULIB_DSIM_Z)==MULIB_DSIM_1);assert(mulib_dsim_resolve(MULIB_DSIM_0,MULIB_DSIM_1)==MULIB_DSIM_X);assert(mulib_dsim_logic4_char(MULIB_DSIM_Z)=='z');
    puts("PASS mulib-dsim-logic4");
    return 0;
}
