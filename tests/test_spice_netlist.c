/* Focused executable regression for mulib-spice-netlist.h. */
#define MULIB_IMPL_SPICE_LEXER
#define MULIB_IMPL_SPICE_NETLIST
#include "mulib-spice-netlist.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_spice_netlist_t n={0};size_t line=0;assert(mulib_spice_netlist_parse("V1 in 0 10\nR1 in out 1k\nR2 out 0 1k\n.end\n",&n,&line));assert(n.parts==3&&n.nodes==2);assert(mulib_spice_netlist_find_node(&n,"out")>0);assert(!mulib_spice_netlist_parse("R1 a 0 nope\n",&n,&line)&&line==1);assert(n.parts==3);
    puts("PASS mulib-spice-netlist");
    return 0;
}
