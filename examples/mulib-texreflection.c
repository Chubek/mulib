/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texreflection.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXREFLECTION
#include "mulib-texreflection.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texreflection_table_t t={0};mulib_texreflection_set(&t,"title","Example");printf("%s\\n",mulib_texreflection_get(&t,"title"));mulib_texreflection_destroy(&t);return 0;}
