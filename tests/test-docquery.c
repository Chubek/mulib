/* Standalone API contract test for mulib-docquery. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCQUERY
#include "mulib-docquery.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_docquery_table_t t={0}; assert(mulib_docquery_set(&t,"example","first"));assert(mulib_docquery_set(&t,"example","second")); assert(!strcmp(mulib_docquery_get(&t,"example"),"second"));assert(mulib_docquery_erase(&t,"example")); assert(!mulib_docquery_get(&t,"example")); mulib_docquery_destroy(&t);return 0;}
