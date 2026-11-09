/* Checked usage example for mulib-ecad-symbol.h. */
#define MULIB_IMPL_ECAD_SYMBOL
#include "mulib-ecad-symbol.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_ecad_symbol_t s = {0};
    assert(mulib_ecad_symbol_pin(&s, "IN", 0, 1, MULIB_ECAD_INPUT));
    assert(mulib_ecad_symbol_pin(&s, "OUT", 1, 1, MULIB_ECAD_OUTPUT));
    assert(!mulib_ecad_symbol_pin(&s, "IN", 0, 1, MULIB_ECAD_INPUT));
    assert(s.pins == 2);
    puts("PASS mulib-ecad-symbol");
    return 0;
}
