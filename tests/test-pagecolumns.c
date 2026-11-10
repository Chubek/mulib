/* Standalone API contract test for mulib-pagecolumns. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGECOLUMNS
#include "mulib-pagecolumns.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pagecolumns_rect_t a={0,0,100,200},out[2];assert(mulib_pagecolumns_divide(a,2,10,out));assert(out[0].width==45&&out[1].x==55);return 0;}
