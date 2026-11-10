/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-tablepaginate.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TABLEPAGINATE
#include "mulib-tablepaginate.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_tablepaginate_list_t l={0};mulib_tablepaginate_push(&l,(mulib_tablepaginate_item_t){10,10,100,30,0});double w=0,h=0;mulib_tablepaginate_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_tablepaginate_destroy(&l);return 0;}
