/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-typography.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPOGRAPHY
#include "mulib-typography.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typography_table_t t={0};mulib_typography_set(&t,"title","Example");printf("%s\\n",mulib_typography_get(&t,"title"));mulib_typography_destroy(&t);return 0;}
