/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-booknav.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_BOOKNAV
#include "mulib-booknav.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_booknav_table_t t={0};mulib_booknav_set(&t,"title","Example");printf("%s\\n",mulib_booknav_get(&t,"title"));mulib_booknav_destroy(&t);return 0;}
