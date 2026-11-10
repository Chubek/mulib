/* Standalone API contract test for mulib-latexwriter. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_LATEXWRITER
#include "mulib-latexwriter.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typecore_buf_t b={0};assert(mulib_latexwriter_document("A&B","x_1",&b));assert(mulib_typecore_ends(&b));assert(strstr((char*)b.data,"A\\&B")&&strstr((char*)b.data,"x\\_1"));mulib_typecore_free(&b);return 0;}
