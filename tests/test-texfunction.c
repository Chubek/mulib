/* Standalone API contract test for mulib-texfunction. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXFUNCTION
#include "mulib-texfunction.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texfunction_table_t t={0}; assert(mulib_texfunction_set(&t,"example","first"));assert(mulib_texfunction_set(&t,"example","second")); assert(!strcmp(mulib_texfunction_get(&t,"example"),"second"));assert(mulib_texfunction_erase(&t,"example")); assert(!mulib_texfunction_get(&t,"example")); mulib_texfunction_destroy(&t);return 0;}
