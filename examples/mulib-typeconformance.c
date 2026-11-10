/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-typeconformance.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPECONFORMANCE
#include "mulib-typeconformance.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typeconformance_table_t t={0};mulib_typeconformance_set(&t,"title","Example");printf("%s\\n",mulib_typeconformance_get(&t,"title"));mulib_typeconformance_destroy(&t);return 0;}
