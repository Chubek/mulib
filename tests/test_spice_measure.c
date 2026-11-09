/* Focused executable regression for mulib-spice-measure.h. */
#define MULIB_IMPL_WAVE_MEASURE
#define MULIB_IMPL_WAVE_SIGNAL
#define MULIB_IMPL_SPICE_MEASURE
#include "mulib-spice-measure.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_wave_signal_t s={0};double v;assert(mulib_wave_signal_push(&s,0,1)&&mulib_wave_signal_push(&s,1,-1));assert(mulib_spice_measure_rms(&s,&v)&&fabs(v-1)<1e-12);mulib_wave_signal_free(&s);
    puts("PASS mulib-spice-measure");
    return 0;
}
