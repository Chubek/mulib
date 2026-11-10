/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-docstyle.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCSTYLE
#include "mulib-docstyle.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_docstyle_table_t t={0};mulib_docstyle_set(&t,"title","Example");printf("%s\\n",mulib_docstyle_get(&t,"title"));mulib_docstyle_destroy(&t);return 0;}
