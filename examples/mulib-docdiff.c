/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-docdiff.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCDIFF
#include "mulib-docdiff.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_docdiff_list_t l={0};mulib_docdiff_push(&l,(mulib_docdiff_item_t){10,10,100,30,0});double w=0,h=0;mulib_docdiff_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_docdiff_destroy(&l);return 0;}
