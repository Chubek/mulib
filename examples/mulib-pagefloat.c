/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-pagefloat.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGEFLOAT
#include "mulib-pagefloat.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pagefloat_list_t l={0};mulib_pagefloat_push(&l,(mulib_pagefloat_item_t){10,10,100,30,0});double w=0,h=0;mulib_pagefloat_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_pagefloat_destroy(&l);return 0;}
