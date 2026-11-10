/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-csl.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_CSL
#include "mulib-csl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_csl_table_t t={0};mulib_csl_set(&t,"title","Example");printf("%s\\n",mulib_csl_get(&t,"title"));mulib_csl_destroy(&t);return 0;}
