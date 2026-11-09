/* Checked usage example for mulib-pcb-bom.h. */
#define MULIB_IMPL_PCB_BOM
#include "mulib-pcb-bom.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_pcb_bom_t b = {0};
    assert(mulib_pcb_bom_add(&b, "ACME", "R10K", 2));
    assert(mulib_pcb_bom_add(&b, "ACME", "R10K", 3) && b.count == 1 && b.lines[0].quantity == 5);
    assert(!mulib_pcb_bom_add(&b, "ACME", "R10K", 0));
    puts("PASS mulib-pcb-bom");
    return 0;
}
