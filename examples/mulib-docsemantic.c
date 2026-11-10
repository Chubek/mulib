/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-docsemantic.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCSEMANTIC
#include "mulib-docsemantic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_docsemantic_table_t t={0};mulib_docsemantic_set(&t,"title","Example");printf("%s\\n",mulib_docsemantic_get(&t,"title"));mulib_docsemantic_destroy(&t);return 0;}
