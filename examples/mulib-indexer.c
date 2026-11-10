/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-indexer.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_INDEXER
#include "mulib-indexer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_indexer_table_t t={0};mulib_indexer_set(&t,"title","Example");printf("%s\\n",mulib_indexer_get(&t,"title"));mulib_indexer_destroy(&t);return 0;}
