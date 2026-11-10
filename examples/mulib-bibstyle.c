/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-bibstyle.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_BIBSTYLE
#include "mulib-bibstyle.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_bibstyle_table_t t={0};mulib_bibstyle_set(&t,"title","Example");printf("%s\\n",mulib_bibstyle_get(&t,"title"));mulib_bibstyle_destroy(&t);return 0;}
