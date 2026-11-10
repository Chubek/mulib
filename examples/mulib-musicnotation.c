/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-musicnotation.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MUSICNOTATION
#include "mulib-musicnotation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_musicnotation_list_t l={0};mulib_musicnotation_push(&l,(mulib_musicnotation_item_t){10,10,100,30,0});double w=0,h=0;mulib_musicnotation_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_musicnotation_destroy(&l);return 0;}
