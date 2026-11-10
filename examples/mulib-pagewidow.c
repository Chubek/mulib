/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-pagewidow.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGEWIDOW
#include "mulib-pagewidow.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pagewidow_list_t l={0};mulib_pagewidow_push(&l,(mulib_pagewidow_item_t){10,10,100,30,0});double w=0,h=0;mulib_pagewidow_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_pagewidow_destroy(&l);return 0;}
