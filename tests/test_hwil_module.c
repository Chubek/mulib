/* Focused executable regression for mulib-hwil-module.h. */
#define MULIB_IMPL_DSIM_LOGIC4
#define MULIB_IMPL_HWIL_MODULE
#include "mulib-hwil-module.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_hwil_module_t m={0};uint16_t a,b,c;mulib_dsim_logic4_t in[]={MULIB_DSIM_1,MULIB_DSIM_0},v[512];assert(mulib_hwil_module_add(&m,MULIB_HWIL_INPUT,0,0,0,&a));assert(mulib_hwil_module_add(&m,MULIB_HWIL_INPUT,0,0,0,&b));assert(mulib_hwil_module_add(&m,MULIB_HWIL_XOR,a,b,0,&c));assert(mulib_hwil_module_eval(&m,in,2,v,512)&&v[c]==MULIB_DSIM_1);assert(!mulib_hwil_module_add(&m,MULIB_HWIL_AND,500,0,0,NULL));
    puts("PASS mulib-hwil-module");
    return 0;
}
