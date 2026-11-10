/* Standalone API contract test for mulib-mathfont. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHFONT
#include "mulib-mathfont.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_mathfont_table_t t={0}; assert(mulib_mathfont_set(&t,"example","first"));assert(mulib_mathfont_set(&t,"example","second")); assert(!strcmp(mulib_mathfont_get(&t,"example"),"second"));assert(mulib_mathfont_erase(&t,"example")); assert(!mulib_mathfont_get(&t,"example")); mulib_mathfont_destroy(&t);return 0;}
