/* Standalone API contract test for mulib-pagefootnote. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGEFOOTNOTE
#include "mulib-pagefootnote.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pagefootnote_list_t l={0};assert(mulib_pagefootnote_push(&l,(mulib_pagefootnote_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_pagefootnote_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_pagefootnote_remove(&l,0));mulib_pagefootnote_destroy(&l);return 0;}
