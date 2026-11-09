/* Checked usage example for mulib-pcb-drc.h. */
#define MULIB_IMPL_PCB_GEOMETRY
#define MULIB_IMPL_PCB_DRC
#include "mulib-pcb-drc.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_pcb_segment_t t[2] = {{{0, 0}, {10, 0}, 1, 1}, {{5, -5}, {5, 5}, 1, 2}};
    mulib_pcb_drc_violation_t v[1];
    assert(mulib_pcb_drc_segments(t, 2, .2, v, 1) == 1 && v[0].first == 0 && v[0].second == 1);
    t[1].net = 1;
    assert(mulib_pcb_drc_segments(t, 2, .2, v, 1) == 0);
    assert(mulib_pcb_drc_segments(NULL, 1, .2, v, 1) == SIZE_MAX);
    puts("PASS mulib-pcb-drc");
    return 0;
}
