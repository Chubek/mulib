/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-pagebib.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGEBIB
#include "mulib-pagebib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pagebib_table_t t={0};mulib_pagebib_set(&t,"title","Example");printf("%s\\n",mulib_pagebib_get(&t,"title"));mulib_pagebib_destroy(&t);return 0;}
