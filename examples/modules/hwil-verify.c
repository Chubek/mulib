/* Checked usage example for mulib-hwil-verify.h. */
#define MULIB_IMPL_DSIM_LOGIC4
#define MULIB_IMPL_HWIL_MODULE
#define MULIB_IMPL_HWIL_VERIFY
#include "mulib-hwil-verify.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_hwil_module_t m = {0};
    size_t bad = 999;
    assert(mulib_hwil_module_add(&m, MULIB_HWIL_INPUT, 0, 0, 0, NULL));
    assert(mulib_hwil_module_verify(&m, &bad));
    m.nodes[0].op = MULIB_HWIL_NOT;
    m.nodes[0].a = 0;
    assert(!mulib_hwil_module_verify(&m, &bad) && bad == 0);
    puts("PASS mulib-hwil-verify");
    return 0;
}
