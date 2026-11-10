/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-typestatus.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPESTATUS
#include "mulib-typestatus.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){assert(!strcmp(mulib_typestatus_name(mulib_typestatus_UNSUPPORTED),"unsupported feature"));return 0;}
