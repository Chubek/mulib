/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texscope.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXSCOPE
#include "mulib-texscope.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texscope_table_t t={0};mulib_texscope_set(&t,"title","Example");printf("%s\\n",mulib_texscope_get(&t,"title"));mulib_texscope_destroy(&t);return 0;}
