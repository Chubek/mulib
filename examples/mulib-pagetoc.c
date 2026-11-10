/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-pagetoc.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGETOC
#include "mulib-pagetoc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pagetoc_table_t t={0};mulib_pagetoc_set(&t,"title","Example");printf("%s\\n",mulib_pagetoc_get(&t,"title"));mulib_pagetoc_destroy(&t);return 0;}
