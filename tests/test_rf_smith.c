/* Focused executable regression for mulib-rf-smith.h. */
#define MULIB_IMPL_RF_SPARAMS
#define MULIB_IMPL_RF_SMITH
#include "mulib-rf-smith.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
double x,y;assert(mulib_rf_smith_xy((mulib_rf_complex_t){50,0},50,&x,&y)&&fabs(x)<1e-12&&fabs(y)<1e-12);
    puts("PASS mulib-rf-smith");
    return 0;
}
