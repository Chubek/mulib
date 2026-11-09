/* Checked usage example for mulib-pcb-pnp.h. */
#define MULIB_IMPL_PCB_PNP
#include "mulib-pcb-pnp.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    FILE *f = tmpfile();
    assert(f);
    mulib_pcb_placement_t p = {"R1", 1, 2, 90, 0};
    assert(mulib_pcb_pnp_write(f, &p, 1));
    rewind(f);
    char buf[256] = {0};
    fread(buf, 1, sizeof buf - 1, f);
    assert(strstr(buf, "R1,1.000000,2.000000,90.000000,top"));
    fclose(f);
    puts("PASS mulib-pcb-pnp");
    return 0;
}
