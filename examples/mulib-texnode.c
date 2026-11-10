/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texnode.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXNODE
#include "mulib-texnode.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texnode_list_t l={0};mulib_texnode_push(&l,(mulib_texnode_item_t){10,10,100,30,0});double w=0,h=0;mulib_texnode_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_texnode_destroy(&l);return 0;}
