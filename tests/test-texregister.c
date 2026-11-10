/* Standalone API contract test for mulib-texregister. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXREGISTER
#include "mulib-texregister.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texregister_table_t t={0}; assert(mulib_texregister_set(&t,"example","first"));assert(mulib_texregister_set(&t,"example","second")); assert(!strcmp(mulib_texregister_get(&t,"example"),"second"));assert(mulib_texregister_erase(&t,"example")); assert(!mulib_texregister_get(&t,"example")); mulib_texregister_destroy(&t);return 0;}
