/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texinsert.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXINSERT
#include "mulib-texinsert.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texinsert_table_t t={0};mulib_texinsert_set(&t,"title","Example");printf("%s\\n",mulib_texinsert_get(&t,"title"));mulib_texinsert_destroy(&t);return 0;}
