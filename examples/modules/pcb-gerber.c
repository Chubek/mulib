/* Checked usage example for mulib-pcb-gerber.h. */
#define MULIB_IMPL_PCB_GEOMETRY
#define MULIB_IMPL_PCB_GERBER
#include "mulib-pcb-gerber.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    FILE *f = tmpfile();
    assert(f);
    mulib_pcb_segment_t s = {{0, 0}, {1, 2}, .25, 1};
    assert(mulib_pcb_gerber_write(f, &s, 1));
    fflush(f);
    rewind(f);
    char buf[512] = {0};
    fread(buf, 1, sizeof buf - 1, f);
    assert(strstr(buf, "%FSLAX46Y46*%") && strstr(buf, "D01*"));
    fclose(f);
    puts("PASS mulib-pcb-gerber");
    return 0;
}
