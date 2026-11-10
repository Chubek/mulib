/* Standalone API contract test for mulib-texreflection. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXREFLECTION
#include "mulib-texreflection.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texreflection_table_t t={0}; assert(mulib_texreflection_set(&t,"example","first"));assert(mulib_texreflection_set(&t,"example","second")); assert(!strcmp(mulib_texreflection_get(&t,"example"),"second"));assert(mulib_texreflection_erase(&t,"example")); assert(!mulib_texreflection_get(&t,"example")); mulib_texreflection_destroy(&t);return 0;}
