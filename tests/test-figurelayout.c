/* Standalone API contract test for mulib-figurelayout. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_FIGURELAYOUT
#include "mulib-figurelayout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_figurelayout_list_t l={0};assert(mulib_figurelayout_push(&l,(mulib_figurelayout_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_figurelayout_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_figurelayout_remove(&l,0));mulib_figurelayout_destroy(&l);return 0;}
