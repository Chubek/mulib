/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-locale.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_LOCALE
#include "mulib-locale.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_locale_table_t t={0};mulib_locale_set(&t,"title","Example");printf("%s\\n",mulib_locale_get(&t,"title"));mulib_locale_destroy(&t);return 0;}
