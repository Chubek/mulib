/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-vectorpath.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_VECTORPATH
#include "mulib-vectorpath.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_vectorpath_list_t l={0};mulib_vectorpath_push(&l,(mulib_vectorpath_item_t){10,10,100,30,0});double w=0,h=0;mulib_vectorpath_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_vectorpath_destroy(&l);return 0;}
