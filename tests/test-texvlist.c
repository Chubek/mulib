/* Standalone API contract test for mulib-texvlist. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXVLIST
#include "mulib-texvlist.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texvlist_list_t l={0};assert(mulib_texvlist_push(&l,(mulib_texvlist_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_texvlist_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_texvlist_remove(&l,0));mulib_texvlist_destroy(&l);return 0;}
