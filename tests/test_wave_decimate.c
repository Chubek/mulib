/* Focused executable regression for mulib-wave-decimate.h. */
#define MULIB_IMPL_WAVE_SIGNAL
#define MULIB_IMPL_WAVE_DECIMATE
#include "mulib-wave-decimate.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_wave_signal_t s={0};mulib_wave_sample_t lo[2],hi[2];for(int i=0;i<6;i++)assert(mulib_wave_signal_push(&s,(double)i,i%3));assert(mulib_wave_decimate_minmax(&s,2,lo,hi));assert(lo[0].value==0&&hi[0].value==2&&lo[1].value==0&&hi[1].value==2);mulib_wave_signal_free(&s);
    puts("PASS mulib-wave-decimate");
    return 0;
}
