/* Standalone API contract test for mulib-mathdelimiter. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHDELIMITER
#include "mulib-mathdelimiter.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_mathdelimiter_list_t l={0};assert(mulib_mathdelimiter_push(&l,(mulib_mathdelimiter_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_mathdelimiter_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_mathdelimiter_remove(&l,0));mulib_mathdelimiter_destroy(&l);return 0;}
