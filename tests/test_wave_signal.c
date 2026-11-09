/* Focused executable regression for mulib-wave-signal.h. */
#define MULIB_IMPL_WAVE_SIGNAL
#include "mulib-wave-signal.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_wave_signal_t s={0};assert(mulib_wave_signal_push(&s,0,1));assert(mulib_wave_signal_push(&s,.1,2));assert(!mulib_wave_signal_push(&s,-1,2));assert(s.count==2);mulib_wave_signal_free(&s);
    puts("PASS mulib-wave-signal");
    return 0;
}
