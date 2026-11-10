/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texmark.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXMARK
#include "mulib-texmark.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texmark_table_t t={0};mulib_texmark_set(&t,"title","Example");printf("%s\\n",mulib_texmark_get(&t,"title"));mulib_texmark_destroy(&t);return 0;}
