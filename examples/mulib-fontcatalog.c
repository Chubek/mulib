/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-fontcatalog.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_FONTCATALOG
#include "mulib-fontcatalog.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_fontcatalog_table_t t={0};mulib_fontcatalog_set(&t,"title","Example");printf("%s\\n",mulib_fontcatalog_get(&t,"title"));mulib_fontcatalog_destroy(&t);return 0;}
