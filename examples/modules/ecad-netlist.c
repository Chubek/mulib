/* Checked usage example for mulib-ecad-netlist.h. */
#define MULIB_IMPL_ECAD_SCHEMATIC
#define MULIB_IMPL_ECAD_NETLIST
#include "mulib-ecad-netlist.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_ecad_schematic_t s = {0};
    mulib_ecad_component_t c = {"R1", 'R', "a", "0", 1000};
    char out[200] = {0};
    size_t n = 0;
    assert(mulib_ecad_schematic_add(&s, &c));
    assert(mulib_ecad_netlist_write(&s, out, sizeof out, &n) && strstr(out, "R1 a 0 1000") &&
           strstr(out, ".end"));
    assert(!mulib_ecad_netlist_write(&s, out, 5, &n));
    puts("PASS mulib-ecad-netlist");
    return 0;
}
