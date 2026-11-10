/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-mathoperator.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHOPERATOR
#include "mulib-mathoperator.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_mathoperator_table_t t={0};mulib_mathoperator_set(&t,"title","Example");printf("%s\\n",mulib_mathoperator_get(&t,"title"));mulib_mathoperator_destroy(&t);return 0;}
