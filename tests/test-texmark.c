/* Standalone API contract test for mulib-texmark. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXMARK
#include "mulib-texmark.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texmark_table_t t={0}; assert(mulib_texmark_set(&t,"example","first"));assert(mulib_texmark_set(&t,"example","second")); assert(!strcmp(mulib_texmark_get(&t,"example"),"second"));assert(mulib_texmark_erase(&t,"example")); assert(!mulib_texmark_get(&t,"example")); mulib_texmark_destroy(&t);return 0;}
