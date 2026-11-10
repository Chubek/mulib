/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texleaders.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXLEADERS
#include "mulib-texleaders.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texleaders_list_t l={0};mulib_texleaders_push(&l,(mulib_texleaders_item_t){10,10,100,30,0});double w=0,h=0;mulib_texleaders_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_texleaders_destroy(&l);return 0;}
