/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-typecache.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPECACHE
#include "mulib-typecache.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typecache_table_t t={0};mulib_typecache_set(&t,"title","Example");printf("%s\\n",mulib_typecache_get(&t,"title"));mulib_typecache_destroy(&t);return 0;}
