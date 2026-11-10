/* Standalone API contract test for mulib-mathatom. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHATOM
#include "mulib-mathatom.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_mathatom_list_t l={0};assert(mulib_mathatom_push(&l,(mulib_mathatom_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_mathatom_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_mathatom_remove(&l,0));mulib_mathatom_destroy(&l);return 0;}
