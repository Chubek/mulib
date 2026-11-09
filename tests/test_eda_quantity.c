/* Focused executable regression for mulib-eda-quantity.h. */
#define MULIB_IMPL_EDA_UNITS
#define MULIB_IMPL_EDA_QUANTITY
#include "mulib-eda-quantity.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_eda_quantity_t a={0},b={0},c={0};a.value=3;a.sigma=.2;b.value=4;b.sigma=.3;assert(mulib_eda_quantity_add(a,b,&c)&&c.value==7);b.dim.exp[0]=1;assert(!mulib_eda_quantity_add(a,b,&c));assert(mulib_eda_quantity_mul(a,b,&c)&&c.dim.exp[0]==1&&c.value==12);
    puts("PASS mulib-eda-quantity");
    return 0;
}
