/* HarfBuzz is a test oracle only; the native-only executable does not load it. */
#define MULIB_FONTSHAPE_ENABLE_NATIVE
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_SFNT
#define MULIB_IMPL_FONTCMAP
#define MULIB_IMPL_FONTGSUB
#define MULIB_IMPL_FONTGPOS
#define MULIB_IMPL_FONTGDEF
#define MULIB_IMPL_FONTMETRICS
#define MULIB_IMPL_FONTKERN
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_FONTSHAPE
#include "mulib-fontshape.h"
#include <assert.h>

static void compare_(const char *path,int scale,int liga,int kern,const char *text,size_t length,
                     mulib_fontshape_t *native,mulib_fontshape_t *reference) {
    const char *features[]={liga?"liga=1":"liga=0",kern?"kern=1":"kern=0"};
    const char *oracle[]={features[0],features[1],"ccmp=0","locl=0","rlig=0","clig=0","calt=0"};
    mulib_fontshape_options_t a={"ltr","Latn","und",features,2},b={"ltr","Latn","und",oracle,7};
    mulib_fontshape_run_t x={0},y={0};
    assert(!mulib_fontshape_shape(native,text,length,&a,&x));
    assert(!mulib_fontshape_shape(reference,text,length,&b,&y));
    assert(x.count==y.count);
    for(size_t i=0;i<x.count;i++) {
        if(x.glyphs[i].glyph!=y.glyphs[i].glyph||x.glyphs[i].cluster!=y.glyphs[i].cluster||
           x.glyphs[i].x_advance!=y.glyphs[i].x_advance||x.glyphs[i].y_advance!=y.glyphs[i].y_advance||
           x.glyphs[i].x_offset!=y.glyphs[i].x_offset||x.glyphs[i].y_offset!=y.glyphs[i].y_offset) {
            fprintf(stderr,"native/HarfBuzz mismatch: %s scale=%d liga=%d kern=%d glyph=%zu\n"
                    "native %u %u %d %d %d %d; oracle %u %u %d %d %d %d\n",
                    path,scale,liga,kern,i,(unsigned)x.glyphs[i].glyph,(unsigned)x.glyphs[i].cluster,
                    x.glyphs[i].x_advance,x.glyphs[i].y_advance,x.glyphs[i].x_offset,x.glyphs[i].y_offset,
                    (unsigned)y.glyphs[i].glyph,(unsigned)y.glyphs[i].cluster,y.glyphs[i].x_advance,
                    y.glyphs[i].y_advance,y.glyphs[i].x_offset,y.glyphs[i].y_offset);
            abort();
        }
    }
    mulib_fontshape_run_destroy(&x);mulib_fontshape_run_destroy(&y);
}
int main(void) {
    const char *paths[]={"/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
                         "/usr/share/fonts/liberation/LiberationSerif-Regular.ttf",
                         "/usr/share/fonts/TTF/DejaVuSans.ttf",
                         "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"};
    const char *texts[]={"office","ffi fi fl ff","AVATAR To Wa","affinity","Hello, world!","A\0V"};
    const int scales[]={2048,1024,1000,97};size_t compared=0;
    for(size_t p=0;p<sizeof(paths)/sizeof(paths[0]);p++)for(size_t s=0;s<sizeof(scales)/sizeof(scales[0]);s++) {
        mulib_fontshape_t *native=mulib_fontshape_open_native(paths[p],0,scales[s]);
        if(!native)continue;
        mulib_fontshape_t *reference=mulib_fontshape_open(paths[p],0,scales[s],NULL);
        if(!reference){mulib_fontshape_close(native);continue;}
        for(int liga=0;liga<2;liga++)for(int kern=0;kern<2;kern++)
            for(size_t i=0;i<sizeof(texts)/sizeof(texts[0]);i++) {
                compare_(paths[p],scales[s],liga,kern,texts[i],i==5?3:strlen(texts[i]),native,reference);
                compared++;
            }
        mulib_fontshape_close(native);mulib_fontshape_close(reference);
    }
    if(!compared)puts("SKIP: native/HarfBuzz comparison needs a sample font and HarfBuzz runtime.");
    else printf("Native Latin/HarfBuzz: %zu glyph-run comparisons passed.\n",compared);
    return 0;
}
