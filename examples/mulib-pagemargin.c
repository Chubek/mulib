/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-pagemargin.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGEMARGIN
#include "mulib-pagemargin.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pagemargin_list_t l={0};mulib_pagemargin_push(&l,(mulib_pagemargin_item_t){10,10,100,30,0});double w=0,h=0;mulib_pagemargin_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_pagemargin_destroy(&l);return 0;}
