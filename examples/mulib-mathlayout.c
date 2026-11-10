/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-mathlayout.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHLAYOUT
#include "mulib-mathlayout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_mathlayout_list_t l={0};mulib_mathlayout_push(&l,(mulib_mathlayout_item_t){10,10,100,30,0});double w=0,h=0;mulib_mathlayout_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_mathlayout_destroy(&l);return 0;}
