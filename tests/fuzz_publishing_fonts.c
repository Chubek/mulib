/* Bounded, external-runtime-free libFuzzer target for the native font path. */
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
#include <assert.h>

int LLVMFuzzerTestOneInput(const uint8_t *data,size_t size) {
    if(size>1024u*1024u)return 0;
    mulib_fontcmap_map_t cmap;uint32_t glyph=0;
    if(mulib_fontcmap_open_subtable(&cmap,data,size)||mulib_fontcmap_open(&cmap,data,size)) {
        (void)mulib_fontcmap_glyph(&cmap,'A',&glyph);
        (void)mulib_fontcmap_glyph(&cmap,0x1f600,&glyph);
        (void)mulib_fontcmap_variation(&cmap,data,size,'A',0xfe0f,&glyph);
    }
    uint32_t glyphs[]={1,2,3,5};size_t consumed=0;uint16_t kind=0;int16_t kern=0;
    mulib_fontgsub_view_t sub;mulib_fontgpos_view_t pos;mulib_fontgdef_view_t def;
    mulib_fontgpos_adjustment_t a,b;
    if(mulib_fontgsub_open(&sub,data,size))
        (void)mulib_fontgsub_substitute(&sub,0,glyphs,4,&glyph,&consumed);
    if(mulib_fontgpos_open(&pos,data,size))
        (void)mulib_fontgpos_pair(&pos,0,3,5,&a,&b,&consumed);
    if(mulib_fontgdef_open(&def,data,size))(void)mulib_fontgdef_glyph_class(&def,3,&kind);
    (void)mulib_fontkern_pair(data,size,3,5,&kern);
    mulib_fontshape_t *font=mulib_fontshape_open_memory(data,size,0,1000);
    if(font) {
        mulib_fontshape_run_t first={0},second={0};
        int one=mulib_fontshape_shape(font,"office AV fi",12,NULL,&first);
        int two=mulib_fontshape_shape(font,"office AV fi",12,NULL,&second);
        assert(one==two&&first.count==second.count);
        for(size_t i=0;i<first.count;i++) {
            assert(first.glyphs[i].glyph==second.glyphs[i].glyph&&first.glyphs[i].cluster==second.glyphs[i].cluster);
            assert(first.glyphs[i].x_advance==second.glyphs[i].x_advance&&first.glyphs[i].y_advance==second.glyphs[i].y_advance);
            assert(first.glyphs[i].x_offset==second.glyphs[i].x_offset&&first.glyphs[i].y_offset==second.glyphs[i].y_offset);
        }
        mulib_fontshape_extents_t extents;mulib_fontshape_metrics_t metrics;
        (void)mulib_fontshape_extents(font,3,&extents);(void)mulib_fontshape_metrics(font,&metrics);
        (void)mulib_fontshape_covers(font,'A');
        mulib_fontshape_run_destroy(&first);mulib_fontshape_run_destroy(&second);
        mulib_fontshape_close(font);
    }
    return 0;
}
