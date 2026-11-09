/* Checked usage example for mulib-fmt-aiger.h. */
#define MULIB_IMPL_SYNTH_AIG
#define MULIB_IMPL_FMT_AIGER
#include "mulib-fmt-aiger.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_synth_aig_t g = {0};
    uint16_t a, b, c;
    FILE *f = tmpfile();
    assert(f);
    assert(mulib_synth_aig_input(&g, &a) && mulib_synth_aig_input(&g, &b) &&
           mulib_synth_aig_and(&g, a, b, &c));
    assert(mulib_fmt_aiger_write_ascii(f, &g, c));
    rewind(f);
    char buf[256] = {0};
    fread(buf, 1, sizeof buf - 1, f);
    assert(strstr(buf, "aag 3 2 0 1 1") && strstr(buf, "6 2 4"));
    fclose(f);
    puts("PASS mulib-fmt-aiger");
    return 0;
}
