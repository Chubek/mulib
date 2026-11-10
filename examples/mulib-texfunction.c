/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texfunction.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXFUNCTION
#include "mulib-texfunction.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texfunction_table_t t={0};mulib_texfunction_set(&t,"title","Example");printf("%s\\n",mulib_texfunction_get(&t,"title"));mulib_texfunction_destroy(&t);return 0;}
