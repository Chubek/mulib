/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-tablelayout.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TABLELAYOUT
#include "mulib-tablelayout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_tablelayout_list_t l={0};mulib_tablelayout_push(&l,(mulib_tablelayout_item_t){10,10,100,30,0});double w=0,h=0;mulib_tablelayout_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_tablelayout_destroy(&l);return 0;}
