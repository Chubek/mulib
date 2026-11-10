/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-mathfont.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHFONT
#include "mulib-mathfont.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_mathfont_table_t t={0};mulib_mathfont_set(&t,"title","Example");printf("%s\\n",mulib_mathfont_get(&t,"title"));mulib_mathfont_destroy(&t);return 0;}
