/* Standalone API contract test for mulib-texoutput. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXOUTPUT
#include "mulib-texoutput.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texoutput_table_t t={0}; assert(mulib_texoutput_set(&t,"example","first"));assert(mulib_texoutput_set(&t,"example","second")); assert(!strcmp(mulib_texoutput_get(&t,"example"),"second"));assert(mulib_texoutput_erase(&t,"example")); assert(!mulib_texoutput_get(&t,"example")); mulib_texoutput_destroy(&t);return 0;}
