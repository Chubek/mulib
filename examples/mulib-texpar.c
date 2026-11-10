/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texpar.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXPAR
#include "mulib-texpar.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texpar_list_t l={0};mulib_texpar_push(&l,(mulib_texpar_item_t){10,10,100,30,0});double w=0,h=0;mulib_texpar_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_texpar_destroy(&l);return 0;}
