/* Standalone API contract test for mulib-texclosure. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXCLOSURE
#include "mulib-texclosure.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texclosure_table_t t={0}; assert(mulib_texclosure_set(&t,"example","first"));assert(mulib_texclosure_set(&t,"example","second")); assert(!strcmp(mulib_texclosure_get(&t,"example"),"second"));assert(mulib_texclosure_erase(&t,"example")); assert(!mulib_texclosure_get(&t,"example")); mulib_texclosure_destroy(&t);return 0;}
