/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-fontsubset.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_FONTSUBSET
#include "mulib-fontsubset.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_fontsubset_list_t l={0};mulib_fontsubset_push(&l,(mulib_fontsubset_item_t){10,10,100,30,0});double w=0,h=0;mulib_fontsubset_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_fontsubset_destroy(&l);return 0;}
