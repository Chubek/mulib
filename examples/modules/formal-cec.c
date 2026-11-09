/* Checked usage example for mulib-formal-cec.h. */
#define MULIB_IMPL_SYNTH_TRUTHTABLE
#define MULIB_IMPL_FORMAL_CEC
#include "mulib-formal-cec.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_synth_truth_t a, b, c;
    assert(mulib_synth_truth_variable(2, 0, &a));
    b = a;
    assert(mulib_formal_truth_equivalent(a, b));
    assert(mulib_synth_truth_variable(2, 1, &c));
    assert(!mulib_formal_truth_equivalent(a, c));
    puts("PASS mulib-formal-cec");
    return 0;
}
