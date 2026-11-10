/* Standalone API contract test for mulib-texvalue. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXVALUE
#include "mulib-texvalue.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texvalue_value_t a={0},b={0};assert(mulib_texvalue_string(&a,"hello"));assert(mulib_texvalue_copy(&b,&a));assert(!strcmp(a.string,b.string));mulib_texvalue_destroy(&a);mulib_texvalue_destroy(&b);return 0;}
