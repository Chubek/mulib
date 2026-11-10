/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-pagebalance.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGEBALANCE
#include "mulib-pagebalance.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pagebalance_list_t l={0};mulib_pagebalance_push(&l,(mulib_pagebalance_item_t){10,10,100,30,0});double w=0,h=0;mulib_pagebalance_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_pagebalance_destroy(&l);return 0;}
