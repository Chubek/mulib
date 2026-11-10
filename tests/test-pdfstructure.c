/* Standalone API contract test for mulib-pdfstructure. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PDFSTRUCTURE
#include "mulib-pdfstructure.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pdfstructure_table_t t={0}; assert(mulib_pdfstructure_set(&t,"example","first"));assert(mulib_pdfstructure_set(&t,"example","second")); assert(!strcmp(mulib_pdfstructure_get(&t,"example"),"second"));assert(mulib_pdfstructure_erase(&t,"example")); assert(!mulib_pdfstructure_get(&t,"example")); mulib_pdfstructure_destroy(&t);return 0;}
