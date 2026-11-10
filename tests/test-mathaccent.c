/* Standalone API contract test for mulib-mathaccent. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHACCENT
#include "mulib-mathaccent.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_mathaccent_list_t l={0};assert(mulib_mathaccent_push(&l,(mulib_mathaccent_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_mathaccent_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_mathaccent_remove(&l,0));mulib_mathaccent_destroy(&l);return 0;}
