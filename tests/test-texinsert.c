/* Standalone API contract test for mulib-texinsert. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXINSERT
#include "mulib-texinsert.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texinsert_table_t t={0}; assert(mulib_texinsert_set(&t,"example","first"));assert(mulib_texinsert_set(&t,"example","second")); assert(!strcmp(mulib_texinsert_get(&t,"example"),"second"));assert(mulib_texinsert_erase(&t,"example")); assert(!mulib_texinsert_get(&t,"example")); mulib_texinsert_destroy(&t);return 0;}
