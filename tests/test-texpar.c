/* Standalone API contract test for mulib-texpar. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXPAR
#include "mulib-texpar.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texpar_list_t l={0};assert(mulib_texpar_push(&l,(mulib_texpar_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_texpar_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_texpar_remove(&l,0));mulib_texpar_destroy(&l);return 0;}
