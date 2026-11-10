/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-pdfstructure.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PDFSTRUCTURE
#include "mulib-pdfstructure.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pdfstructure_table_t t={0};mulib_pdfstructure_set(&t,"title","Example");printf("%s\\n",mulib_pdfstructure_get(&t,"title"));mulib_pdfstructure_destroy(&t);return 0;}
