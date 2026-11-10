/* Standalone API contract test for mulib-texscope. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXSCOPE
#include "mulib-texscope.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texscope_table_t t={0}; assert(mulib_texscope_set(&t,"example","first"));assert(mulib_texscope_set(&t,"example","second")); assert(!strcmp(mulib_texscope_get(&t,"example"),"second"));assert(mulib_texscope_erase(&t,"example")); assert(!mulib_texscope_get(&t,"example")); mulib_texscope_destroy(&t);return 0;}
