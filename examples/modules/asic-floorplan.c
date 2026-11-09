/* Checked usage example for mulib-asic-floorplan.h. */
#define MULIB_IMPL_ASIC_FLOORPLAN
#include "mulib-asic-floorplan.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_asic_rect_t r[2] = {{0, 0, 2, 1}, {0, 0, 2, 1}};
    assert(mulib_asic_floorplan_place_row(r, 2, 1, 3, .5, 2));
    assert(r[0].x == 1 && r[1].x == 3.5 && r[0].y == 3);
    assert(!mulib_asic_floorplan_overlaps(r[0], r[1]));
    puts("PASS mulib-asic-floorplan");
    return 0;
}
