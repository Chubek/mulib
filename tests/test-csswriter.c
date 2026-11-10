/* Standalone API contract test for mulib-csswriter. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_CSSWRITER
#include "mulib-csswriter.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typecore_buf_t b={0};assert(mulib_csswriter_write_cstr(&b,"A&B"));assert(b.size>0);mulib_typecore_free(&b);return 0;}
