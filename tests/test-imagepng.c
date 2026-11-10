/* Standalone API contract test for mulib-imagepng. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_IMAGEPNG
#include "mulib-imagepng.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){const unsigned char rgb[]={255,0,0};mulib_typecore_buf_t b={0};assert(mulib_imagepng_encode_rgb(rgb,1,1,&b));assert(b.size>60&&b.data[0]==137&&!memcmp(b.data+12,"IHDR",4));mulib_typecore_free(&b);return 0;}
