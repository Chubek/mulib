/* Focused executable regression for mulib-hdl-source.h. */
#define MULIB_IMPL_HDL_SOURCE
#include "mulib-hdl-source.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
const char *source="module a;\nwire x;";mulib_hdl_source_t s={source,strlen(source)};size_t line,col;assert(mulib_hdl_source_linecol(s,10,&line,&col)&&line==2&&col==1);assert(!mulib_hdl_source_linecol(s,100,&line,&col));
    puts("PASS mulib-hdl-source");
    return 0;
}
