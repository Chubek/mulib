/* Focused executable regression for mulib-eda-units.h. */
#define MULIB_IMPL_EDA_UNITS
#include "mulib-eda-units.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_eda_unit_t u;double x=0;mulib_eda_dimension_t dim;assert(mulib_eda_unit_lookup("V",&u));assert(mulib_eda_parse_engineering("2.5kOhm",&x,&dim));assert(fabs(x-2500)<1e-10);assert(mulib_eda_convert(0,"degC","K",&x)&&fabs(x-273.15)<1e-9);assert(!mulib_eda_convert(2,"V","A",&x));assert(!mulib_eda_parse_engineering("3ZV",&x,&dim));
    puts("PASS mulib-eda-units");
    return 0;
}
