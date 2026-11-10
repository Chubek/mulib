/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-mathatom.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHATOM
#include "mulib-mathatom.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_mathatom_list_t l={0};mulib_mathatom_push(&l,(mulib_mathatom_item_t){10,10,100,30,0});double w=0,h=0;mulib_mathatom_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_mathatom_destroy(&l);return 0;}
