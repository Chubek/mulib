/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-unicodecase.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_UNICODECASE
#include "mulib-unicodecase.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_unicodecase_list_t l={0};mulib_unicodecase_push(&l,(mulib_unicodecase_item_t){10,10,100,30,0});double w=0,h=0;mulib_unicodecase_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_unicodecase_destroy(&l);return 0;}
