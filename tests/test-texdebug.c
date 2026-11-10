/* Standalone API contract test for mulib-texdebug. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXDEBUG
#include "mulib-texdebug.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texdebug_table_t t={0}; assert(mulib_texdebug_set(&t,"example","first"));assert(mulib_texdebug_set(&t,"example","second")); assert(!strcmp(mulib_texdebug_get(&t,"example"),"second"));assert(mulib_texdebug_erase(&t,"example")); assert(!mulib_texdebug_get(&t,"example")); mulib_texdebug_destroy(&t);return 0;}
