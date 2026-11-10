/* Standalone API contract test for mulib-pdfoutline. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PDFOUTLINE
#include "mulib-pdfoutline.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pdfoutline_table_t t={0}; assert(mulib_pdfoutline_set(&t,"example","first"));assert(mulib_pdfoutline_set(&t,"example","second")); assert(!strcmp(mulib_pdfoutline_get(&t,"example"),"second"));assert(mulib_pdfoutline_erase(&t,"example")); assert(!mulib_pdfoutline_get(&t,"example")); mulib_pdfoutline_destroy(&t);return 0;}
