/* Checked usage example for mulib-formal-sat.h. */
#define MULIB_IMPL_FORMAL_CNF
#define MULIB_IMPL_FORMAL_SAT
#include "mulib-formal-sat.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_formal_cnf_t c = {0};
    int8_t model[33] = {0}, a[] = {1}, b[] = {-1};
    assert(mulib_formal_cnf_add(&c, a, 1));
    assert(mulib_formal_sat_solve(&c, model, 100) == MULIB_FORMAL_SAT_SAT && model[1] == 1);
    assert(mulib_formal_cnf_add(&c, b, 1));
    assert(mulib_formal_sat_solve(&c, model, 100) == MULIB_FORMAL_SAT_UNSAT);
    assert(mulib_formal_sat_solve(&c, model, 0) == MULIB_FORMAL_SAT_UNKNOWN);
    puts("PASS mulib-formal-sat");
    return 0;
}
