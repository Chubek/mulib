/* Checked usage example for mulib-fmt-sv.h. */
#define MULIB_IMPL_DSIM_LOGIC4
#define MULIB_IMPL_HWIL_MODULE
#define MULIB_IMPL_FMT_SV
#include "mulib-fmt-sv.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_hwil_module_t m = {0};
    uint16_t a, b, c;
    assert(mulib_hwil_module_add(&m, MULIB_HWIL_INPUT, 0, 0, 0, &a) &&
           mulib_hwil_module_add(&m, MULIB_HWIL_INPUT, 0, 0, 0, &b) &&
           mulib_hwil_module_add(&m, MULIB_HWIL_AND, a, b, 0, &c));
    FILE *f = tmpfile();
    assert(f);
    assert(mulib_fmt_sv_emit(f, &m, c));
    rewind(f);
    char text[512] = {0};
    fread(text, 1, sizeof text - 1, f);
    assert(strstr(text, "module amb3r_generated") && strstr(text, "n0 & n1"));
    fclose(f);
    puts("PASS mulib-fmt-sv");
    return 0;
}
