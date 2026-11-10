/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-docnode.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCNODE
#include "mulib-docnode.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_docnode_list_t l={0};mulib_docnode_push(&l,(mulib_docnode_item_t){10,10,100,30,0});double w=0,h=0;mulib_docnode_bounds(&l,&w,&h);printf("extent %.0fx%.0f\\n",w,h);mulib_docnode_destroy(&l);return 0;}
