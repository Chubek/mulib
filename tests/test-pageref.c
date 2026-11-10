/* Standalone API contract test for mulib-pageref. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGEREF
#include "mulib-pageref.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pageref_table_t t={0}; assert(mulib_pageref_set(&t,"example","first"));assert(mulib_pageref_set(&t,"example","second")); assert(!strcmp(mulib_pageref_get(&t,"example"),"second"));assert(mulib_pageref_erase(&t,"example")); assert(!mulib_pageref_get(&t,"example")); mulib_pageref_destroy(&t);return 0;}
