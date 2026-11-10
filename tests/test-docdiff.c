/* Standalone API contract test for mulib-docdiff. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCDIFF
#include "mulib-docdiff.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_docdiff_list_t l={0};assert(mulib_docdiff_push(&l,(mulib_docdiff_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_docdiff_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_docdiff_remove(&l,0));mulib_docdiff_destroy(&l);return 0;}
