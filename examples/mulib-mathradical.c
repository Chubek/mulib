/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-mathradical.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHRADICAL
#include "mulib-mathradical.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){double a=0;assert(mulib_mathradical_ascent(10,1,2,&a)&&a==13);return 0;}
