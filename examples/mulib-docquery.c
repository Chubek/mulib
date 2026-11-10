/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-docquery.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCQUERY
#include "mulib-docquery.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_docquery_table_t t={0};mulib_docquery_set(&t,"title","Example");printf("%s\\n",mulib_docquery_get(&t,"title"));mulib_docquery_destroy(&t);return 0;}
