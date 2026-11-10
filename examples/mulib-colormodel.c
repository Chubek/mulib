/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-colormodel.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_COLORMODEL
#include "mulib-colormodel.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){double r=0;if(mulib_colormodel_compute(8,2,&r))printf("result %.3f\\n",r);return 0;}
