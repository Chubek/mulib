/* Standalone API contract test for mulib-pagebalance. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGEBALANCE
#include "mulib-pagebalance.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pagebalance_list_t l={0};assert(mulib_pagebalance_push(&l,(mulib_pagebalance_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_pagebalance_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_pagebalance_remove(&l,0));mulib_pagebalance_destroy(&l);return 0;}
