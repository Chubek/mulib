/* Standalone API contract test for mulib-texglue. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXGLUE
#include "mulib-texglue.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texglue_glue_t a[]={{10,5,2,0,0},{20,5,3,0,0}};int64_t b[2];assert(mulib_texglue_adjust(a,2,40,b));assert(b[0]==15&&b[1]==25);return 0;}
