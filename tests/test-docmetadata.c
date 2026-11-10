/* Standalone API contract test for mulib-docmetadata. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCMETADATA
#include "mulib-docmetadata.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_docmetadata_table_t t={0}; assert(mulib_docmetadata_set(&t,"example","first"));assert(mulib_docmetadata_set(&t,"example","second")); assert(!strcmp(mulib_docmetadata_get(&t,"example"),"second"));assert(mulib_docmetadata_erase(&t,"example")); assert(!mulib_docmetadata_get(&t,"example")); mulib_docmetadata_destroy(&t);return 0;}
