/* Standalone API contract test for mulib-mathmatrix. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHMATRIX
#include "mulib-mathmatrix.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_mathmatrix_list_t l={0};assert(mulib_mathmatrix_push(&l,(mulib_mathmatrix_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_mathmatrix_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_mathmatrix_remove(&l,0));mulib_mathmatrix_destroy(&l);return 0;}
