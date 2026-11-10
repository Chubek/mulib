/* Demonstration of independent, bounded publication output writers.
 * Build: cc -std=c11 -Wall -Wextra -Iinclude examples/build-publication.c -lm -o publish-example
 * Run: ./publish-example /tmp/mulib-published
 * IMPORTANT: these are minimal target-format examples, not a full typesetter.
 */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXEXPR
#define MULIB_IMPL_PDFCANVAS
#define MULIB_IMPL_DVI
#define MULIB_IMPL_EPUBWRITER
#define MULIB_IMPL_HTMLWRITER
#define MULIB_IMPL_LATEXWRITER
#define MULIB_IMPL_IMAGEPNG
#define MULIB_IMPL_MOBIWRITE
#include "mulib-texexpr.h"
#include "mulib-pdfcanvas.h"
#include "mulib-dvi.h"
#include "mulib-epubwriter.h"
#include "mulib-htmlwriter.h"
#include "mulib-latexwriter.h"
#include "mulib-imagepng.h"
#include "mulib-mobiwrite.h"
#include <stdio.h>
#include <string.h>
static int write_file(const char *dir,const char *name,const mulib_typecore_buf_t *b){
    char path[2048];int n=snprintf(path,sizeof(path),"%s/%s",dir,name);
    if(n<0||(size_t)n>=sizeof(path))return 0;
    FILE *f=fopen(path,"wb");if(!f)return 0;
    int ok=fwrite(b->data,1,b->size,f)==b->size&&fclose(f)==0;
    if(!ok)remove(path);
    return ok;
}
int main(int argc,char **argv){
    if(argc!=2){fprintf(stderr,"usage: %s EXISTING_OUTPUT_DIRECTORY\n",argv[0]);return 2;}
    const char *title="MuLIB Publishing",*body="Hello from the independent MuLIB publication baselines!";
    mulib_typecore_buf_t b={0};double arithmetic=0;int32_t height[]={65536},width[]={3*65536};
    const unsigned char rgb[3]={0x32,0x88,0xc4};
    if(!mulib_texexpr_evaluate("(2+3)*4",&arithmetic)||arithmetic!=20)return 1;
    if(!mulib_htmlwriter_document(title,body,&b)||!write_file(argv[1],"document.html",&b))return 1;
    mulib_typecore_free(&b);
    if(!mulib_latexwriter_document(title,body,&b)||!write_file(argv[1],"document.tex",&b))return 1;
    mulib_typecore_free(&b);
    if(!mulib_pdfcanvas_write_text("MuLIB publishing test",72,720,18,&b)||!write_file(argv[1],"document.pdf",&b))return 1;
    mulib_typecore_free(&b);
    if(!mulib_dvi_write_rules(height,width,1,&b)||!write_file(argv[1],"document.dvi",&b))return 1;
    mulib_typecore_free(&b);
    if(!mulib_epubwriter_book(title,body,&b)||!write_file(argv[1],"document.epub",&b))return 1;
    mulib_typecore_free(&b);
    if(!mulib_imagepng_encode_rgb(rgb,1,1,&b)||!write_file(argv[1],"image.png",&b))return 1;
    mulib_typecore_free(&b);
    if(!mulib_mobiwrite_palmdoc("MuLIB sample",body,strlen(body),&b)||!write_file(argv[1],"document.pdb",&b))return 1;
    mulib_typecore_free(&b);
    puts("Generated HTML, LaTeX, PDF, DVI, EPUB, PNG and PalmDOC examples.");
    puts("MOBI and AZW3 are not fully implemented by these demo writers.");
    return 0;
}
