/* Checked usage example for mulib-formal-cnf.h. */
#define MULIB_IMPL_FORMAL_CNF
#include "mulib-formal-cnf.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_formal_cnf_t c = {0};
    int8_t one[] = {1}, two[] = {-1, 2};
    assert(mulib_formal_cnf_add(&c, one, 1) && mulib_formal_cnf_add(&c, two, 2));
    assert(c.vars == 2 && c.count == 2);
    int8_t invalid[] = {0};
    assert(!mulib_formal_cnf_add(&c, invalid, 1));
    puts("PASS mulib-formal-cnf");
    return 0;
}
