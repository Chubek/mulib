/* Standalone API contract test for mulib-pdfannot. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PDFANNOT
#include "mulib-pdfannot.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pdfannot_table_t t={0}; assert(mulib_pdfannot_set(&t,"example","first"));assert(mulib_pdfannot_set(&t,"example","second")); assert(!strcmp(mulib_pdfannot_get(&t,"example"),"second"));assert(mulib_pdfannot_erase(&t,"example")); assert(!mulib_pdfannot_get(&t,"example")); mulib_pdfannot_destroy(&t);return 0;}
