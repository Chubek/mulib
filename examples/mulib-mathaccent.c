/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-mathaccent.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHACCENT
#include "mulib-mathaccent.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_mathaccent_list_t l={0};mulib_mathaccent_push(&l,(mulib_mathaccent_item_t){10,10,100,30,0});double w=0,h=0;mulib_mathaccent_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_mathaccent_destroy(&l);return 0;}
