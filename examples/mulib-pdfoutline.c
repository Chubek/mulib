/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-pdfoutline.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PDFOUTLINE
#include "mulib-pdfoutline.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pdfoutline_table_t t={0};mulib_pdfoutline_set(&t,"title","Example");printf("%s\\n",mulib_pdfoutline_get(&t,"title"));mulib_pdfoutline_destroy(&t);return 0;}
