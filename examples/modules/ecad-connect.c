/* Checked usage example for mulib-ecad-connect.h. */
#define MULIB_IMPL_ECAD_SCHEMATIC
#define MULIB_IMPL_ECAD_CONNECT
#include "mulib-ecad-connect.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_ecad_schematic_t s = {0};
    size_t nodes, ground;
    mulib_ecad_component_t a = {"R1", 'R', "in", "0", 1000}, b = {"R2", 'R', "in", "out", 500};
    assert(mulib_ecad_schematic_add(&s, &a) && mulib_ecad_schematic_add(&s, &b));
    assert(mulib_ecad_connect_count_nodes(&s, &nodes, &ground) && nodes == 2 && ground == 1);
    puts("PASS mulib-ecad-connect");
    return 0;
}
