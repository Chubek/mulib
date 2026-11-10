/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-microtype.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MICROTYPE
#include "mulib-microtype.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){double r=0;if(mulib_microtype_compute(8,2,&r))printf("result %.3f\\n",r);return 0;}
