/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texffi.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXFFI
#include "mulib-texffi.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texffi_table_t t={0};mulib_texffi_set(&t,"title","Example");printf("%s\\n",mulib_texffi_get(&t,"title"));mulib_texffi_destroy(&t);return 0;}
