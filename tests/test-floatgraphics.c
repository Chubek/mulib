/* Standalone API contract test for mulib-floatgraphics. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_FLOATGRAPHICS
#include "mulib-floatgraphics.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_floatgraphics_list_t l={0};assert(mulib_floatgraphics_push(&l,(mulib_floatgraphics_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_floatgraphics_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_floatgraphics_remove(&l,0));mulib_floatgraphics_destroy(&l);return 0;}
