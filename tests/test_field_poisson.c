/* Focused executable regression for mulib-field-poisson.h. */
#define MULIB_IMPL_FIELD_POISSON
#include "mulib-field-poisson.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
double phi;assert(mulib_field_poisson_1d(1,2,8,0,0,.5,&phi)&&fabs(phi-.5)<1e-12);assert(!mulib_field_poisson_1d(1,0,8,0,0,.5,&phi));
    puts("PASS mulib-field-poisson");
    return 0;
}
