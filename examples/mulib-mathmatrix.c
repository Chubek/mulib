/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-mathmatrix.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHMATRIX
#include "mulib-mathmatrix.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_mathmatrix_list_t l={0};mulib_mathmatrix_push(&l,(mulib_mathmatrix_item_t){10,10,100,30,0});double w=0,h=0;mulib_mathmatrix_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_mathmatrix_destroy(&l);return 0;}
