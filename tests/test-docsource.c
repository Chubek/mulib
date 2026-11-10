/* Standalone API contract test for mulib-docsource. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCSOURCE
#include "mulib-docsource.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_docsource_table_t t={0}; assert(mulib_docsource_set(&t,"example","first"));assert(mulib_docsource_set(&t,"example","second")); assert(!strcmp(mulib_docsource_get(&t,"example"),"second"));assert(mulib_docsource_erase(&t,"example")); assert(!mulib_docsource_get(&t,"example")); mulib_docsource_destroy(&t);return 0;}
