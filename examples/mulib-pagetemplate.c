/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-pagetemplate.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGETEMPLATE
#include "mulib-pagetemplate.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pagetemplate_table_t t={0};mulib_pagetemplate_set(&t,"title","Example");printf("%s\\n",mulib_pagetemplate_get(&t,"title"));mulib_pagetemplate_destroy(&t);return 0;}
