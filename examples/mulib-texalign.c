/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texalign.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXALIGN
#include "mulib-texalign.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texalign_list_t l={0};mulib_texalign_push(&l,(mulib_texalign_item_t){10,10,100,30,0});double w=0,h=0;mulib_texalign_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_texalign_destroy(&l);return 0;}
