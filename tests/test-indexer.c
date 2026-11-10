/* Standalone API contract test for mulib-indexer. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_INDEXER
#include "mulib-indexer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_indexer_table_t t={0}; assert(mulib_indexer_set(&t,"example","first"));assert(mulib_indexer_set(&t,"example","second")); assert(!strcmp(mulib_indexer_get(&t,"example"),"second"));assert(mulib_indexer_erase(&t,"example")); assert(!mulib_indexer_get(&t,"example")); mulib_indexer_destroy(&t);return 0;}
