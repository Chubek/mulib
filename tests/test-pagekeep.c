/* Standalone API contract test for mulib-pagekeep. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGEKEEP
#include "mulib-pagekeep.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pagekeep_list_t l={0};assert(mulib_pagekeep_push(&l,(mulib_pagekeep_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_pagekeep_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_pagekeep_remove(&l,0));mulib_pagekeep_destroy(&l);return 0;}
