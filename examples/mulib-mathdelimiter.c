/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-mathdelimiter.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHDELIMITER
#include "mulib-mathdelimiter.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_mathdelimiter_list_t l={0};mulib_mathdelimiter_push(&l,(mulib_mathdelimiter_item_t){10,10,100,30,0});double w=0,h=0;mulib_mathdelimiter_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_mathdelimiter_destroy(&l);return 0;}
