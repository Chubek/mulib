/* Standalone API contract test for mulib-pageindex. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGEINDEX
#include "mulib-pageindex.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pageindex_table_t t={0}; assert(mulib_pageindex_set(&t,"example","first"));assert(mulib_pageindex_set(&t,"example","second")); assert(!strcmp(mulib_pageindex_get(&t,"example"),"second"));assert(mulib_pageindex_erase(&t,"example")); assert(!mulib_pageindex_get(&t,"example")); mulib_pageindex_destroy(&t);return 0;}
