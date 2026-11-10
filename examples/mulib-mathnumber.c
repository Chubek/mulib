/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-mathnumber.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHNUMBER
#include "mulib-mathnumber.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_mathnumber_table_t t={0};mulib_mathnumber_set(&t,"title","Example");printf("%s\\n",mulib_mathnumber_get(&t,"title"));mulib_mathnumber_destroy(&t);return 0;}
