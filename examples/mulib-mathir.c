/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-mathir.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHIR
#include "mulib-mathir.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_mathir_list_t l={0};mulib_mathir_push(&l,(mulib_mathir_item_t){10,10,100,30,0});double w=0,h=0;mulib_mathir_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_mathir_destroy(&l);return 0;}
