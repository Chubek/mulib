/* Focused executable regression for mulib-hwil-types.h. */
#define MULIB_IMPL_HWIL_TYPES
#include "mulib-hwil-types.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_hwil_type_t a={MULIB_HWIL_BITS,32,1,1},b=a;assert(mulib_hwil_type_valid(a)&&mulib_hwil_type_equal(a,b));b.width=0;assert(!mulib_hwil_type_valid(b));
    puts("PASS mulib-hwil-types");
    return 0;
}
