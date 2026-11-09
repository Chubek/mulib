/* Checked usage example for mulib-pcb-excellon.h. */
#define MULIB_IMPL_PCB_EXCELLON
#include "mulib-pcb-excellon.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    FILE *f = tmpfile();
    assert(f);
    mulib_pcb_drill_t h = {1.0, 2.0, .8};
    assert(mulib_pcb_excellon_write(f, &h, 1));
    fflush(f);
    rewind(f);
    char buf[512] = {0};
    fread(buf, 1, sizeof buf - 1, f);
    assert(strstr(buf, "M48") && strstr(buf, "T01C0.800") && strstr(buf, "M30"));
    fclose(f);
    puts("PASS mulib-pcb-excellon");
    return 0;
}
