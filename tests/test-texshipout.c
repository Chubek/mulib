/* Standalone API contract test for mulib-texshipout. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXSHIPOUT
#include "mulib-texshipout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texshipout_list_t l={0};assert(mulib_texshipout_push(&l,(mulib_texshipout_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_texshipout_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_texshipout_remove(&l,0));mulib_texshipout_destroy(&l);return 0;}
