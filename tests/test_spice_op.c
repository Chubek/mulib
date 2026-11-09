/* Focused executable regression for mulib-spice-op.h. */
#define MULIB_IMPL_NUM_DENSE
#define MULIB_IMPL_SPICE_INDEPENDENT
#define MULIB_IMPL_SPICE_LEXER
#define MULIB_IMPL_SPICE_NETLIST
#define MULIB_IMPL_SPICE_R
#define MULIB_IMPL_SPICE_STAMP
#define MULIB_IMPL_SPICE_OP
#include "mulib-spice-op.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_spice_netlist_t net={0};double v[10]={0};assert(mulib_spice_netlist_parse("V1 in 0 10\nR1 in out 1k\nR2 out 0 1k\n",&net,NULL));assert(mulib_spice_op_solve(&net,v,10));int out=mulib_spice_netlist_find_node(&net,"out");assert(out>0&&fabs(v[out-1]-5)<1e-9);assert(mulib_spice_netlist_parse("R1 floating 0 1k\nR2 disconnected x 2k\n",&net,NULL));assert(!mulib_spice_op_solve(&net,v,10));
    puts("PASS mulib-spice-op");
    return 0;
}
