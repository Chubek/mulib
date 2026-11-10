/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-pageref.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGEREF
#include "mulib-pageref.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pageref_table_t t={0};mulib_pageref_set(&t,"title","Example");printf("%s\\n",mulib_pageref_get(&t,"title"));mulib_pageref_destroy(&t);return 0;}
