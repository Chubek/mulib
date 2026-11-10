/* Standalone API contract test for mulib-texhlist. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXHLIST
#include "mulib-texhlist.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texhlist_list_t l={0};assert(mulib_texhlist_push(&l,(mulib_texhlist_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_texhlist_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_texhlist_remove(&l,0));mulib_texhlist_destroy(&l);return 0;}
