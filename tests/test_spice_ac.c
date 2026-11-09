/* Focused executable regression for mulib-spice-ac.h. */
#define MULIB_IMPL_SPICE_LEXER
#define MULIB_IMPL_SPICE_NETLIST
#define MULIB_IMPL_SPICE_AC
#include "mulib-spice-ac.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_spice_netlist_t n={0};mulib_spice_complex_t v[3]={0};assert(mulib_spice_netlist_parse("V1 in 0 1\nR1 in out 1k\nC1 out 0 1u\n",&n,NULL));double f=1/(2*3.14159265358979323846*.001);int k;assert(mulib_spice_ac_solve(&n,f,v,3));k=mulib_spice_netlist_find_node(&n,"out");assert(k>0&&fabs(v[k-1].re-.5)<1e-8&&fabs(v[k-1].im+.5)<1e-8);
    puts("PASS mulib-spice-ac");
    return 0;
}
