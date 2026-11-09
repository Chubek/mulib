/* Checked usage example for mulib-field-thermal.h. */
#define MULIB_IMPL_FIELD_THERMAL
#include "mulib-field-thermal.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    double t;
    assert(mulib_field_thermal_1d(1, 1, 8, 300, 300, .5, &t) && fabs(t - 301) < 1e-12);
    assert(!mulib_field_thermal_1d(1, 0, 8, 300, 300, .5, &t));
    puts("PASS mulib-field-thermal");
    return 0;
}
