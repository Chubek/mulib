/* Focused executable regression for mulib-synth-aig.h. */
#define MULIB_IMPL_SYNTH_AIG
#include "mulib-synth-aig.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_synth_aig_t g={0};uint16_t a,b,c,d;int o;uint8_t in[]={1,0};assert(mulib_synth_aig_input(&g,&a)&&mulib_synth_aig_input(&g,&b));assert(mulib_synth_aig_and(&g,a,b,&c)&&mulib_synth_aig_and(&g,b,a,&d)&&c==d);assert(mulib_synth_aig_eval(&g,in,2,c,&o)&&o==0);in[1]=1;assert(mulib_synth_aig_eval(&g,in,2,c,&o)&&o==1);assert(mulib_synth_aig_and(&g,a,a,&d)&&d==a);
    puts("PASS mulib-synth-aig");
    return 0;
}
