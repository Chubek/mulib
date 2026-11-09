/* Focused executable regression for mulib-wave-measure.h. */
#define MULIB_IMPL_WAVE_SIGNAL
#define MULIB_IMPL_WAVE_MEASURE
#include "mulib-wave-measure.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_wave_signal_t s={0};double rms,lo,hi;size_t crossings;assert(mulib_wave_signal_push(&s,0,-2)&&mulib_wave_signal_push(&s,1,2));assert(mulib_wave_measure_rms(&s,&rms)&&fabs(rms-2)<1e-12);assert(mulib_wave_measure_peak(&s,&lo,&hi)&&lo==-2&&hi==2);assert(mulib_wave_measure_crossings(&s,0,&crossings)&&crossings==1);mulib_wave_signal_free(&s);
    puts("PASS mulib-wave-measure");
    return 0;
}
