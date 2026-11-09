/* Checked usage example for mulib-eda-constraints.h. */
#define MULIB_IMPL_EDA_CONSTRAINTS
#include "mulib-eda-constraints.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_eda_constraint_t c = {0};
    double v = -1;
    c.relation = MULIB_EDA_LE;
    c.actual = 12;
    c.target = 10;
    assert(mulib_eda_constraint_check(c, &v) && v == 2);
    c.actual = 9;
    assert(mulib_eda_constraint_check(c, &v) && v == 0);
    c.tolerance = -1;
    assert(!mulib_eda_constraint_check(c, &v));
    puts("PASS mulib-eda-constraints");
    return 0;
}
