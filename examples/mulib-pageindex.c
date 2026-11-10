/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-pageindex.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGEINDEX
#include "mulib-pageindex.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pageindex_table_t t={0};mulib_pageindex_set(&t,"title","Example");printf("%s\\n",mulib_pageindex_get(&t,"title"));mulib_pageindex_destroy(&t);return 0;}
