/* Standalone API contract test for mulib-texleaders. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXLEADERS
#include "mulib-texleaders.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texleaders_list_t l={0};assert(mulib_texleaders_push(&l,(mulib_texleaders_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_texleaders_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_texleaders_remove(&l,0));mulib_texleaders_destroy(&l);return 0;}
