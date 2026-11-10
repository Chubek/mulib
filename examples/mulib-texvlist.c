/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texvlist.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXVLIST
#include "mulib-texvlist.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texvlist_list_t l={0};mulib_texvlist_push(&l,(mulib_texvlist_item_t){10,10,100,30,0});double w=0,h=0;mulib_texvlist_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_texvlist_destroy(&l);return 0;}
