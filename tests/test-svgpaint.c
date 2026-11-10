/* Standalone API contract test for mulib-svgpaint. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_SVGPAINT
#include "mulib-svgpaint.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_svgpaint_list_t l={0};assert(mulib_svgpaint_push(&l,(mulib_svgpaint_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_svgpaint_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_svgpaint_remove(&l,0));mulib_svgpaint_destroy(&l);return 0;}
