/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-toc.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TOC
#include "mulib-toc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_toc_table_t t={0};mulib_toc_set(&t,"title","Example");printf("%s\\n",mulib_toc_get(&t,"title"));mulib_toc_destroy(&t);return 0;}
