/* Checked usage example for mulib-ecad-erc.h. */
#define MULIB_IMPL_ECAD_SCHEMATIC
#define MULIB_IMPL_ECAD_ERC
#include "mulib-ecad-erc.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_ecad_schematic_t s = {0};
    mulib_ecad_erc_result_t r;
    mulib_ecad_component_t c = {"R1", 'R', "a", "a", 1000};
    assert(mulib_ecad_schematic_add(&s, &c));
    assert(mulib_ecad_erc_run(&s, &r) && r.duplicate_shorts == 1 && r.unconnected_ground == 1);
    puts("PASS mulib-ecad-erc");
    return 0;
}
