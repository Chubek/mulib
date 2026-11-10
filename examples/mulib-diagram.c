/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-diagram.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DIAGRAM
#include "mulib-diagram.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_diagram_list_t l={0};mulib_diagram_push(&l,(mulib_diagram_item_t){10,10,100,30,0});double w=0,h=0;mulib_diagram_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_diagram_destroy(&l);return 0;}
