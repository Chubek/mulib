/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-pdfmetadata.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PDFMETADATA
#include "mulib-pdfmetadata.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pdfmetadata_table_t t={0};mulib_pdfmetadata_set(&t,"title","Example");printf("%s\\n",mulib_pdfmetadata_get(&t,"title"));mulib_pdfmetadata_destroy(&t);return 0;}
