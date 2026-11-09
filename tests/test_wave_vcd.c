/* Focused executable regression for mulib-wave-vcd.h. */
#define MULIB_IMPL_DSIM_LOGIC4
#define MULIB_IMPL_WAVE_VCD
#include "mulib-wave-vcd.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
FILE *f=tmpfile();assert(f);mulib_wave_vcd_t v={0};assert(mulib_wave_vcd_begin(&v,f,"clk"));assert(mulib_wave_vcd_at(&v,1,MULIB_DSIM_1));assert(!mulib_wave_vcd_at(&v,0,MULIB_DSIM_0));fflush(f);rewind(f);char buf[1024]={0};size_t n=fread(buf,1,sizeof buf-1,f);assert(n>0&&strstr(buf,"$timescale 1ns")&&strstr(buf,"#1\n1!"));fclose(f);
    puts("PASS mulib-wave-vcd");
    return 0;
}
