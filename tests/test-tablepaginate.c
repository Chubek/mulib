/* Standalone API contract test for mulib-tablepaginate. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TABLEPAGINATE
#include "mulib-tablepaginate.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_tablepaginate_list_t l={0};assert(mulib_tablepaginate_push(&l,(mulib_tablepaginate_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_tablepaginate_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_tablepaginate_remove(&l,0));mulib_tablepaginate_destroy(&l);return 0;}
