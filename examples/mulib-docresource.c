/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-docresource.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCRESOURCE
#include "mulib-docresource.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_docresource_table_t t={0};mulib_docresource_set(&t,"title","Example");printf("%s\\n",mulib_docresource_get(&t,"title"));mulib_docresource_destroy(&t);return 0;}
