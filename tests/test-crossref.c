/* Standalone API contract test for mulib-crossref. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_CROSSREF
#include "mulib-crossref.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_crossref_table_t t={0}; assert(mulib_crossref_set(&t,"example","first"));assert(mulib_crossref_set(&t,"example","second")); assert(!strcmp(mulib_crossref_get(&t,"example"),"second"));assert(mulib_crossref_erase(&t,"example")); assert(!mulib_crossref_get(&t,"example")); mulib_crossref_destroy(&t);return 0;}
