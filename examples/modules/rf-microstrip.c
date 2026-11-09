/* Checked usage example for mulib-rf-microstrip.h. */
#define MULIB_IMPL_RF_MICROSTRIP
#include "mulib-rf-microstrip.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    double z;
    assert(mulib_rf_microstrip_z0(2, 1, 4.2, &z) && z > 40 && z < 90);
    assert(!mulib_rf_microstrip_z0(2, 0, 4.2, &z));
    puts("PASS mulib-rf-microstrip");
    return 0;
}
