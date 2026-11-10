/* Standalone API contract test for mulib-texpatternlib. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXPATTERNLIB
#include "mulib-texpatternlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texpatternlib_table_t t={0}; assert(mulib_texpatternlib_set(&t,"example","first"));assert(mulib_texpatternlib_set(&t,"example","second")); assert(!strcmp(mulib_texpatternlib_get(&t,"example"),"second"));assert(mulib_texpatternlib_erase(&t,"example")); assert(!mulib_texpatternlib_get(&t,"example")); mulib_texpatternlib_destroy(&t);return 0;}
