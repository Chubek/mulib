/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texoutput.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXOUTPUT
#include "mulib-texoutput.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texoutput_table_t t={0};mulib_texoutput_set(&t,"title","Example");printf("%s\\n",mulib_texoutput_get(&t,"title"));mulib_texoutput_destroy(&t);return 0;}
