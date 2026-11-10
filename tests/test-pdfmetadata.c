/* Standalone API contract test for mulib-pdfmetadata. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PDFMETADATA
#include "mulib-pdfmetadata.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pdfmetadata_table_t t={0}; assert(mulib_pdfmetadata_set(&t,"example","first"));assert(mulib_pdfmetadata_set(&t,"example","second")); assert(!strcmp(mulib_pdfmetadata_get(&t,"example"),"second"));assert(mulib_pdfmetadata_erase(&t,"example")); assert(!mulib_pdfmetadata_get(&t,"example")); mulib_pdfmetadata_destroy(&t);return 0;}
