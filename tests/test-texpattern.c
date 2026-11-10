/* Standalone API contract test for mulib-texpattern. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXPATTERN
#include "mulib-texpattern.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texpattern_table_t t={0}; assert(mulib_texpattern_set(&t,"example","first"));assert(mulib_texpattern_set(&t,"example","second")); assert(!strcmp(mulib_texpattern_get(&t,"example"),"second"));assert(mulib_texpattern_erase(&t,"example")); assert(!mulib_texpattern_get(&t,"example")); mulib_texpattern_destroy(&t);return 0;}
