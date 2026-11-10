/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texmodule.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXMODULE
#include "mulib-texmodule.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texmodule_table_t t={0};mulib_texmodule_set(&t,"title","Example");printf("%s\\n",mulib_texmodule_get(&t,"title"));mulib_texmodule_destroy(&t);return 0;}
