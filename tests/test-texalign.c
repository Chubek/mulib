/* Standalone API contract test for mulib-texalign. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXALIGN
#include "mulib-texalign.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texalign_list_t l={0};assert(mulib_texalign_push(&l,(mulib_texalign_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_texalign_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_texalign_remove(&l,0));mulib_texalign_destroy(&l);return 0;}
