/* Focused executable regression for mulib-synth-truthtable.h. */
#define MULIB_IMPL_SYNTH_TRUTHTABLE
#include "mulib-synth-truthtable.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_synth_truth_t a,b,x;int z;assert(mulib_synth_truth_variable(2,0,&a));assert(mulib_synth_truth_variable(2,1,&b));assert(mulib_synth_truth_apply(a,b,'^',&x));assert(mulib_synth_truth_eval(x,1,&z)&&z==1);assert(mulib_synth_truth_eval(x,3,&z)&&z==0);assert(!mulib_synth_truth_variable(7,0,&a));
    puts("PASS mulib-synth-truthtable");
    return 0;
}
