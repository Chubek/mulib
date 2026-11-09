/* Focused executable regression for mulib-fpga-lut.h. */
#define MULIB_IMPL_SYNTH_TRUTHTABLE
#define MULIB_IMPL_FPGA_LUT
#include "mulib-fpga-lut.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_synth_truth_t t;mulib_fpga_lut_t lut;int x;assert(mulib_synth_truth_variable(2,1,&t));assert(mulib_fpga_lut_from_truth(t,&lut));assert(mulib_fpga_lut_eval(lut,2,&x)&&x==1);assert(mulib_fpga_lut_eval(lut,1,&x)&&x==0);
    puts("PASS mulib-fpga-lut");
    return 0;
}
