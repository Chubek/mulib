/* Standalone API contract test for mulib-epubwriter. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_EPUBWRITER
#include "mulib-epubwriter.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typecore_buf_t b={0};assert(mulib_epubwriter_book("Test","one & two",&b));assert(b.size>800&&b.data[0]=='P'&&b.data[1]=='K');mulib_typecore_free(&b);return 0;}
