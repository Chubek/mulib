/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-plot2d.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PLOT2D
#include "mulib-plot2d.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_plot2d_list_t l={0};mulib_plot2d_push(&l,(mulib_plot2d_item_t){10,10,100,30,0});double w=0,h=0;mulib_plot2d_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_plot2d_destroy(&l);return 0;}
