/* Checked usage example for mulib-pcb-impedance.h. */
#define MULIB_IMPL_RF_MICROSTRIP
#define MULIB_IMPL_PCB_IMPEDANCE
#include "mulib-pcb-impedance.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    double z;
    assert(mulib_pcb_microstrip_impedance(2, 1, 4.2, &z) && z > 40 && z < 90);
    assert(!mulib_pcb_microstrip_impedance(0, 1, 4.2, &z));
    puts("PASS mulib-pcb-impedance");
    return 0;
}
