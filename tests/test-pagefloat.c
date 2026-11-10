/* Standalone API contract test for mulib-pagefloat. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGEFLOAT
#include "mulib-pagefloat.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pagefloat_list_t l={0};assert(mulib_pagefloat_push(&l,(mulib_pagefloat_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_pagefloat_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_pagefloat_remove(&l,0));mulib_pagefloat_destroy(&l);return 0;}
