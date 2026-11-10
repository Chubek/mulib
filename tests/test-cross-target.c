#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PDFCANVAS
#define MULIB_IMPL_DVI
#define MULIB_IMPL_EPUBWRITER
#define MULIB_IMPL_HTMLWRITER
#define MULIB_IMPL_LATEXWRITER
#define MULIB_IMPL_IMAGEPNG
#define MULIB_IMPL_MOBIWRITE
#include "mulib-pdfcanvas.h"
#include "mulib-dvi.h"
#include "mulib-epubwriter.h"
#include "mulib-htmlwriter.h"
#include "mulib-latexwriter.h"
#include "mulib-imagepng.h"
#include "mulib-mobiwrite.h"
#include <assert.h>
#include <string.h>
int main(void){
    mulib_typecore_buf_t b={0};
    assert(mulib_pdfcanvas_write_text("Test",72,720,12,&b));
    assert(b.size>300&&!memcmp(b.data,"%PDF",4));mulib_typecore_free(&b);
    int32_t heights[]={65536},widths[]={65536};
    assert(mulib_dvi_write_rules(heights,widths,1,&b));assert(b.data[0]==247);mulib_typecore_free(&b);
    assert(mulib_epubwriter_book("Title","Paragraph",&b));
    assert(b.size>500&&b.data[0]=='P'&&b.data[1]=='K');mulib_typecore_free(&b);
    assert(mulib_htmlwriter_document("T","<&>",&b));assert(mulib_typecore_ends(&b));
    assert(strstr((const char*)b.data,"&lt;&amp;&gt;"));mulib_typecore_free(&b);
    assert(mulib_latexwriter_document("T","x_2",&b));assert(mulib_typecore_ends(&b));
    assert(strstr((const char*)b.data,"x\\_2"));mulib_typecore_free(&b);
    unsigned char color[]={10,20,30};assert(mulib_imagepng_encode_rgb(color,1,1,&b));
    assert(b.data[0]==137);mulib_typecore_free(&b);
    assert(mulib_mobiwrite_palmdoc("T","hello",5,&b));assert(b.size>100);mulib_typecore_free(&b);
    return 0;
}
