/* Focused executable regression for mulib-rf-sparams.h. */
#define MULIB_IMPL_RF_SPARAMS
#include "mulib-rf-sparams.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_rf_complex_t z={100,0},g,out;assert(mulib_rf_s11_from_impedance(z,50,&g));assert(fabs(g.re-1.0/3)<1e-12&&fabs(g.im)<1e-12);assert(mulib_rf_impedance_from_s11(g,50,&out)&&fabs(out.re-100)<1e-10);assert(!mulib_rf_s11_from_impedance(z,0,&g));
    puts("PASS mulib-rf-sparams");
    return 0;
}
