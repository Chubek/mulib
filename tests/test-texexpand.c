/* Standalone API contract test for mulib-texexpand. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXEXPAND
#include "mulib-texexpand.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texexpand_engine_t e={0};assert(mulib_texexpand_define(&e,"name","world"));mulib_typecore_buf_t b={0};assert(mulib_texexpand_once(&e,"hi \\name",&b));assert(mulib_typecore_ends(&b));assert(!strcmp((char*)b.data,"hi world"));mulib_typecore_free(&b);mulib_texexpand_destroy(&e);return 0;}
