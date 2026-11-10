/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-pagefootnote.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGEFOOTNOTE
#include "mulib-pagefootnote.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pagefootnote_list_t l={0};mulib_pagefootnote_push(&l,(mulib_pagefootnote_item_t){10,10,100,30,0});double w=0,h=0;mulib_pagefootnote_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_pagefootnote_destroy(&l);return 0;}
