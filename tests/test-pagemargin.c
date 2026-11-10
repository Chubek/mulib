/* Standalone API contract test for mulib-pagemargin. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGEMARGIN
#include "mulib-pagemargin.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pagemargin_list_t l={0};assert(mulib_pagemargin_push(&l,(mulib_pagemargin_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_pagemargin_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_pagemargin_remove(&l,0));mulib_pagemargin_destroy(&l);return 0;}
