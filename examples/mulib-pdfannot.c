/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-pdfannot.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PDFANNOT
#include "mulib-pdfannot.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pdfannot_table_t t={0};mulib_pdfannot_set(&t,"title","Example");printf("%s\\n",mulib_pdfannot_get(&t,"title"));mulib_pdfannot_destroy(&t);return 0;}
