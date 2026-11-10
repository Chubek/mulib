/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-doclength.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCLENGTH
#include "mulib-doclength.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){double p=0;assert(mulib_doclength_convert(96,mulib_doclength_PX,mulib_doclength_PT,&p));assert(p==72);return 0;}
