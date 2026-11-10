/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texclosure.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXCLOSURE
#include "mulib-texclosure.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texclosure_table_t t={0};mulib_texclosure_set(&t,"title","Example");printf("%s\\n",mulib_texclosure_get(&t,"title"));mulib_texclosure_destroy(&t);return 0;}
