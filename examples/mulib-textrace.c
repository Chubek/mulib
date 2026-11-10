/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-textrace.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXTRACE
#include "mulib-textrace.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_textrace_table_t t={0};mulib_textrace_set(&t,"title","Example");printf("%s\\n",mulib_textrace_get(&t,"title"));mulib_textrace_destroy(&t);return 0;}
