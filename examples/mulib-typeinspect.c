/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-typeinspect.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPEINSPECT
#include "mulib-typeinspect.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typeinspect_table_t t={0};mulib_typeinspect_set(&t,"title","Example");printf("%s\\n",mulib_typeinspect_get(&t,"title"));mulib_typeinspect_destroy(&t);return 0;}
