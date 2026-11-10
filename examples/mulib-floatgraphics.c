/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-floatgraphics.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_FLOATGRAPHICS
#include "mulib-floatgraphics.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_floatgraphics_list_t l={0};mulib_floatgraphics_push(&l,(mulib_floatgraphics_item_t){10,10,100,30,0});double w=0,h=0;mulib_floatgraphics_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_floatgraphics_destroy(&l);return 0;}
