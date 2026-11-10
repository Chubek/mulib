/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texregister.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXREGISTER
#include "mulib-texregister.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texregister_table_t t={0};mulib_texregister_set(&t,"title","Example");printf("%s\\n",mulib_texregister_get(&t,"title"));mulib_texregister_destroy(&t);return 0;}
