/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texpattern.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXPATTERN
#include "mulib-texpattern.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texpattern_table_t t={0};mulib_texpattern_set(&t,"title","Example");printf("%s\\n",mulib_texpattern_get(&t,"title"));mulib_texpattern_destroy(&t);return 0;}
