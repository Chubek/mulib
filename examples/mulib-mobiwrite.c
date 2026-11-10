/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-mobiwrite.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MOBIWRITE
#include "mulib-mobiwrite.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typecore_buf_t b={0};assert(mulib_mobiwrite_palmdoc("book","hello",5,&b));assert(b.size==117&&!memcmp(b.data+60,"TEXtREAd",8));mulib_typecore_free(&b);return 0;}
