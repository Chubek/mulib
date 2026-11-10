/* Standalone API contract test for mulib-tablelayout. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TABLELAYOUT
#include "mulib-tablelayout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_tablelayout_list_t l={0};assert(mulib_tablelayout_push(&l,(mulib_tablelayout_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_tablelayout_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_tablelayout_remove(&l,0));mulib_tablelayout_destroy(&l);return 0;}
