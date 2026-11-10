/* Standalone API contract test for mulib-docresource. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCRESOURCE
#include "mulib-docresource.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_docresource_table_t t={0}; assert(mulib_docresource_set(&t,"example","first"));assert(mulib_docresource_set(&t,"example","second")); assert(!strcmp(mulib_docresource_get(&t,"example"),"second"));assert(mulib_docresource_erase(&t,"example")); assert(!mulib_docresource_get(&t,"example")); mulib_docresource_destroy(&t);return 0;}
