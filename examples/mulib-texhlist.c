/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texhlist.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXHLIST
#include "mulib-texhlist.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texhlist_list_t l={0};mulib_texhlist_push(&l,(mulib_texhlist_item_t){10,10,100,30,0});double w=0,h=0;mulib_texhlist_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_texhlist_destroy(&l);return 0;}
