/* Standalone API contract test for mulib-texmodule. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXMODULE
#include "mulib-texmodule.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texmodule_table_t t={0}; assert(mulib_texmodule_set(&t,"example","first"));assert(mulib_texmodule_set(&t,"example","second")); assert(!strcmp(mulib_texmodule_get(&t,"example"),"second"));assert(mulib_texmodule_erase(&t,"example")); assert(!mulib_texmodule_get(&t,"example")); mulib_texmodule_destroy(&t);return 0;}
