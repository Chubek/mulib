/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-crossref.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_CROSSREF
#include "mulib-crossref.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_crossref_table_t t={0};mulib_crossref_set(&t,"title","Example");printf("%s\\n",mulib_crossref_get(&t,"title"));mulib_crossref_destroy(&t);return 0;}
