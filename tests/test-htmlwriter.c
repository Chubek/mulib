/* Standalone API contract test for mulib-htmlwriter. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_HTMLWRITER
#include "mulib-htmlwriter.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typecore_buf_t b={0};assert(mulib_htmlwriter_document("a<b","x&y",&b));assert(mulib_typecore_ends(&b));assert(strstr((char*)b.data,"a&lt;b")&&strstr((char*)b.data,"x&amp;y"));mulib_typecore_free(&b);return 0;}
