/* Native Latin subset; build with C11 and libc only (no HarfBuzz or libdl). */
#define MULIB_FONTSHAPE_NATIVE_ONLY
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_SFNT
#define MULIB_IMPL_FONTCMAP
#define MULIB_IMPL_FONTGSUB
#define MULIB_IMPL_FONTGPOS
#define MULIB_IMPL_FONTGDEF
#define MULIB_IMPL_FONTMETRICS
#define MULIB_IMPL_FONTKERN
#define MULIB_IMPL_FONTSHAPE
#include "mulib-fontshape.h"
#include <stdio.h>

int main(int argc,char **argv) {
    const char *paths[]={"/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
                         "/usr/share/fonts/TTF/DejaVuSans.ttf",
                         "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"};
    mulib_fontshape_t *font=NULL;
    if(argc>1)font=mulib_fontshape_open_native(argv[1],0,2048);
    else for(size_t i=0;i<sizeof(paths)/sizeof(paths[0])&&!font;i++)
        font=mulib_fontshape_open_native(paths[i],0,2048);
    if(!font) {
        if(argc>1)return 1;
        puts("SKIP: no sample font; pass a static SFNT/TTC font path.");return 0;
    }
    mulib_fontshape_run_t run={0};
    if(mulib_fontshape_shape(font,"office AV",9,NULL,&run)) {
        mulib_fontshape_close(font);return 1;
    }
    for(size_t i=0;i<run.count;i++)
        printf("glyph=%u byte=%u advance=%d\n",(unsigned)run.glyphs[i].glyph,
               (unsigned)run.glyphs[i].cluster,(int)run.glyphs[i].x_advance);
    mulib_fontshape_run_destroy(&run);mulib_fontshape_close(font);return 0;
}
