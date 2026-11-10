/* Standalone API contract test for mulib-verttext. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_VERTTEXT
#include "mulib-verttext.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_verttext_list_t l={0};assert(mulib_verttext_push(&l,(mulib_verttext_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_verttext_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_verttext_remove(&l,0));mulib_verttext_destroy(&l);return 0;}
