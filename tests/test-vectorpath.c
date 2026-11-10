/* Standalone API contract test for mulib-vectorpath. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_VECTORPATH
#include "mulib-vectorpath.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_vectorpath_list_t l={0};assert(mulib_vectorpath_push(&l,(mulib_vectorpath_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_vectorpath_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_vectorpath_remove(&l,0));mulib_vectorpath_destroy(&l);return 0;}
