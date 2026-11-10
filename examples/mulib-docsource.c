/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-docsource.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCSOURCE
#include "mulib-docsource.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_docsource_table_t t={0};mulib_docsource_set(&t,"title","Example");printf("%s\\n",mulib_docsource_get(&t,"title"));mulib_docsource_destroy(&t);return 0;}
