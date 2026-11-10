/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-chemical.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_CHEMICAL
#include "mulib-chemical.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_chemical_list_t l={0};mulib_chemical_push(&l,(mulib_chemical_item_t){10,10,100,30,0});double w=0,h=0;mulib_chemical_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_chemical_destroy(&l);return 0;}
