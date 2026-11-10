/* Standalone API contract test for mulib-mathspeech. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHSPEECH
#include "mulib-mathspeech.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typecore_buf_t b={0};assert(mulib_mathspeech_write_cstr(&b,"A&B"));assert(b.size>0);mulib_typecore_free(&b);return 0;}
