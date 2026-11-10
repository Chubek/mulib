/* Standalone API contract test for mulib-typecache. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPECACHE
#include "mulib-typecache.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typecache_table_t t={0}; assert(mulib_typecache_set(&t,"example","first"));assert(mulib_typecache_set(&t,"example","second")); assert(!strcmp(mulib_typecache_get(&t,"example"),"second"));assert(mulib_typecache_erase(&t,"example")); assert(!mulib_typecache_get(&t,"example")); mulib_typecache_destroy(&t);return 0;}
