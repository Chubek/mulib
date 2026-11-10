/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texdebug.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXDEBUG
#include "mulib-texdebug.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texdebug_table_t t={0};mulib_texdebug_set(&t,"title","Example");printf("%s\\n",mulib_texdebug_get(&t,"title"));mulib_texdebug_destroy(&t);return 0;}
