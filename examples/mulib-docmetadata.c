/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-docmetadata.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCMETADATA
#include "mulib-docmetadata.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_docmetadata_table_t t={0};mulib_docmetadata_set(&t,"title","Example");printf("%s\\n",mulib_docmetadata_get(&t,"title"));mulib_docmetadata_destroy(&t);return 0;}
