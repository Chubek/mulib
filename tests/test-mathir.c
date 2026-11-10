/* Standalone API contract test for mulib-mathir. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHIR
#include "mulib-mathir.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_mathir_list_t l={0};assert(mulib_mathir_push(&l,(mulib_mathir_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_mathir_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_mathir_remove(&l,0));mulib_mathir_destroy(&l);return 0;}
