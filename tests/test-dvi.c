/* Standalone API contract test for mulib-dvi. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DVI
#include "mulib-dvi.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){int32_t h[1]={65536},w[1]={200000};mulib_typecore_buf_t b={0};assert(mulib_dvi_write_rules(h,w,1,&b));assert(b.size>90&&b.data[0]==247);mulib_typecore_free(&b);return 0;}
