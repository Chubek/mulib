/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-verttext.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_VERTTEXT
#include "mulib-verttext.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_verttext_list_t l={0};mulib_verttext_push(&l,(mulib_verttext_item_t){10,10,100,30,0});double w=0,h=0;mulib_verttext_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_verttext_destroy(&l);return 0;}
