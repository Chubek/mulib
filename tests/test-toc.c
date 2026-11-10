/* Standalone API contract test for mulib-toc. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TOC
#include "mulib-toc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_toc_table_t t={0}; assert(mulib_toc_set(&t,"example","first"));assert(mulib_toc_set(&t,"example","second")); assert(!strcmp(mulib_toc_get(&t,"example"),"second"));assert(mulib_toc_erase(&t,"example")); assert(!mulib_toc_get(&t,"example")); mulib_toc_destroy(&t);return 0;}
