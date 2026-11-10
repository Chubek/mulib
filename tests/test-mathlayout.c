/* Standalone API contract test for mulib-mathlayout. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHLAYOUT
#include "mulib-mathlayout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_mathlayout_list_t l={0};assert(mulib_mathlayout_push(&l,(mulib_mathlayout_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_mathlayout_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_mathlayout_remove(&l,0));mulib_mathlayout_destroy(&l);return 0;}
