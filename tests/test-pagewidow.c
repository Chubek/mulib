/* Standalone API contract test for mulib-pagewidow. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGEWIDOW
#include "mulib-pagewidow.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pagewidow_list_t l={0};assert(mulib_pagewidow_push(&l,(mulib_pagewidow_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_pagewidow_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_pagewidow_remove(&l,0));mulib_pagewidow_destroy(&l);return 0;}
