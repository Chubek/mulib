/* Standalone API contract test for mulib-plot2d. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PLOT2D
#include "mulib-plot2d.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_plot2d_list_t l={0};assert(mulib_plot2d_push(&l,(mulib_plot2d_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_plot2d_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_plot2d_remove(&l,0));mulib_plot2d_destroy(&l);return 0;}
