/* Standalone API contract test for mulib-texnode. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXNODE
#include "mulib-texnode.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texnode_list_t l={0};assert(mulib_texnode_push(&l,(mulib_texnode_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_texnode_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_texnode_remove(&l,0));mulib_texnode_destroy(&l);return 0;}
