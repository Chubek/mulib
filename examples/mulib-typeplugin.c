/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-typeplugin.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPEPLUGIN
#include "mulib-typeplugin.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typeplugin_table_t t={0};mulib_typeplugin_set(&t,"title","Example");printf("%s\\n",mulib_typeplugin_get(&t,"title"));mulib_typeplugin_destroy(&t);return 0;}
