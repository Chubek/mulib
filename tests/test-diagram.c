/* Standalone API contract test for mulib-diagram. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DIAGRAM
#include "mulib-diagram.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_diagram_list_t l={0};assert(mulib_diagram_push(&l,(mulib_diagram_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_diagram_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_diagram_remove(&l,0));mulib_diagram_destroy(&l);return 0;}
