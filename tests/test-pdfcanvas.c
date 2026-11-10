/* Standalone API contract test for mulib-pdfcanvas. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PDFCANVAS
#include "mulib-pdfcanvas.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typecore_buf_t b={0};assert(mulib_pdfcanvas_write_text("Hello PDF",50,700,12,&b));assert(b.size>100&&!memcmp(b.data,"%PDF-1.4",8));mulib_typecore_free(&b);return 0;}
