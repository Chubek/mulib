/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-svgpaint.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_SVGPAINT
#include "mulib-svgpaint.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_svgpaint_list_t l={0};mulib_svgpaint_push(&l,(mulib_svgpaint_item_t){10,10,100,30,0});double w=0,h=0;mulib_svgpaint_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_svgpaint_destroy(&l);return 0;}
