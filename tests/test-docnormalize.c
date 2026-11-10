/* Standalone API contract test for mulib-docnormalize. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCNORMALIZE
#include "mulib-docnormalize.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typecore_buf_t b={0};assert(mulib_docnormalize_write_cstr(&b,"A&B"));assert(b.size>0);mulib_typecore_free(&b);return 0;}
