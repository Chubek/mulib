/* Standalone API contract test for mulib-texffi. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXFFI
#include "mulib-texffi.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texffi_table_t t={0}; assert(mulib_texffi_set(&t,"example","first"));assert(mulib_texffi_set(&t,"example","second")); assert(!strcmp(mulib_texffi_get(&t,"example"),"second"));assert(mulib_texffi_erase(&t,"example")); assert(!mulib_texffi_get(&t,"example")); mulib_texffi_destroy(&t);return 0;}
