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
#include "fixtures/publishing-font.h"
#include <assert.h>

int main(void) {
    unsigned char bytes[1024],collection[1044];size_t size=mulib_test_font_make_(bytes);
    mulib_fontshape_t *f=mulib_fontshape_open_memory(bytes,size,0,1000);assert(f);
    memset(bytes,0,sizeof(bytes)); /* open_memory owns a copy. */
    assert(mulib_fontshape_scale(f)==1000&&!mulib_fontshape_native(f));
    assert(mulib_fontshape_covers(f,'f')==1&&mulib_fontshape_covers(f,'z')==0);
    assert(mulib_fontshape_covers(f,0xd800)==-1&&mulib_fontshape_covers(f,0x110000)==-1);
    mulib_fontshape_metrics_t metrics;
    assert(!mulib_fontshape_metrics(f,&metrics)&&metrics.ascender==800&&metrics.descender==-200&&metrics.line_gap==100);
    mulib_fontshape_run_t run={0};
    assert(!mulib_fontshape_shape(f,"fiAV",4,NULL,&run)&&run.count==3);
    assert(run.glyphs[0].glyph==4&&run.glyphs[0].cluster==0&&run.glyphs[0].x_advance==900);
    assert(run.glyphs[1].glyph==3&&run.glyphs[1].cluster==2&&run.glyphs[1].x_advance==650);
    assert(run.glyphs[2].glyph==5&&run.glyphs[2].cluster==3&&run.glyphs[2].x_advance==700);
    assert(mulib_fontshape_shape(f,"fi",2,NULL,&run)==-1&&run.count==3);
    mulib_fontshape_run_destroy(&run);
    const char *disabled[]={"liga=0","kern=0"};mulib_fontshape_options_t options={"ltr","Latn","en",disabled,2};
    assert(!mulib_fontshape_shape(f,"fiAV",4,&options,&run)&&run.count==4&&run.glyphs[2].x_advance==700);
    mulib_fontshape_run_destroy(&run);
    assert(!mulib_fontshape_shape(f,NULL,0,NULL,&run)&&!run.glyphs&&!run.count);
    assert(mulib_fontshape_shape(f,NULL,1,NULL,&run)==-1);
    assert(mulib_fontshape_shape(f,"\xc0\x80",2,NULL,&run)==-1);
    assert(mulib_fontshape_shape(f,"\xd8\xb3",2,NULL,&run)==-1); /* no fake Arabic shaping */
    assert(mulib_fontshape_shape(f,"\t",1,NULL,&run)==-1);
    assert(mulib_fontshape_shape(f,"a",SIZE_MAX,NULL,&run)==-1);
    options.direction="rtl";assert(mulib_fontshape_shape(f,"fi",2,&options,&run)==-1);
    options.direction="ltr";options.script="Arab";assert(mulib_fontshape_shape(f,"fi",2,&options,&run)==-1);
    options.script="Latn";options.language="tr";assert(mulib_fontshape_shape(f,"fi",2,&options,&run)==-1);
    options.language="en";const char *bad[]={"liga[0:1]=0"};options.features=bad;options.feature_count=1;
    assert(mulib_fontshape_shape(f,"fi",2,&options,&run)==-1&&!run.glyphs&&!run.count);
    assert(!mulib_fontshape_variations(f,NULL,0));bad[0]="wght=700";
    assert(mulib_fontshape_variations(f,bad,1)==-1);
    mulib_fontshape_extents_t extents={1,2,3,4};
    assert(mulib_fontshape_extents(f,1,&extents)==-1&&extents.x_bearing==1);
    int32_t caret_sentinel=0,*carets=&caret_sentinel;size_t count=123;
    assert(!mulib_fontshape_carets(f,4,"ltr",&carets,&count)&&!carets&&!count);
    mulib_fontshape_carets_destroy(carets);
    assert(mulib_fontshape_carets(f,4,"rtl",&carets,&count)==-1);
    mulib_fontshape_close(f);mulib_fontshape_close(NULL);mulib_fontshape_run_destroy(NULL);
    assert(!mulib_fontshape_open_native("/nonexistent-font",0,1000));
    assert(!mulib_fontshape_open_memory(NULL,10,0,1000));
    size=mulib_test_font_make_(bytes);
    assert(!mulib_fontshape_open_memory(bytes,size,0,0));
    assert(!mulib_fontshape_open_memory(bytes,64u*1024u*1024u+1,0,1000));
    assert(!mulib_fontshape_open_memory(bytes,size,1,1000));
    /* TTC with two shared faces, absolute offsets, and a rejected third index. */
    memset(collection,0,sizeof(collection));memcpy(collection,"ttcf",4);
    mulib_test_font_u32_(collection,4,0x10000);mulib_test_font_u32_(collection,8,2);
    mulib_test_font_u32_(collection,12,20);mulib_test_font_u32_(collection,16,20);
    memcpy(collection+20,bytes,size);
    for(size_t i=0;i<8;i++) {
        uint32_t offset;mulib_typecore_slice_t s={bytes,size};
        assert(mulib_typecore_read32be(s,12+i*16+8,&offset));
        mulib_test_font_u32_(collection,20+12+i*16+8,offset+20);
    }
    f=mulib_fontshape_open_memory(collection,size+20,1,2000);assert(f);
    assert(!mulib_fontshape_shape(f,"fiAV",4,NULL,&run)&&run.glyphs[1].x_advance==1300);
    mulib_fontshape_close(f);assert(run.glyphs[0].glyph==4);mulib_fontshape_run_destroy(&run);
    assert(!mulib_fontshape_open_memory(collection,size+20,2,1000));
    /* TTC v2 adds a checked DSIG descriptor before the face directory. */
    memset(collection,0,sizeof(collection));memcpy(collection,"ttcf",4);
    mulib_test_font_u32_(collection,4,0x20000);mulib_test_font_u32_(collection,8,2);
    mulib_test_font_u32_(collection,12,32);mulib_test_font_u32_(collection,16,32);
    memcpy(collection+32,bytes,size);
    for(size_t i=0;i<8;i++) {
        uint32_t offset;mulib_typecore_slice_t s={bytes,size};
        assert(mulib_typecore_read32be(s,12+i*16+8,&offset));
        mulib_test_font_u32_(collection,32+12+i*16+8,offset+32);
    }
    f=mulib_fontshape_open_memory(collection,size+32,0,1000);assert(f);
    mulib_fontshape_close(f);mulib_test_font_u32_(collection,24,UINT32_MAX);
    assert(!mulib_fontshape_open_memory(collection,size+32,0,1000));
    for(size_t i=0;i<size;i++) {f=mulib_fontshape_open_memory(bytes,i,0,1000);mulib_fontshape_close(f);}
    /* Overlapping tables and truncated metric records cannot be opened. */
    mulib_test_font_u32_(bytes,12+16+8,140);
    assert(!mulib_fontshape_open_memory(bytes,size,0,1000));
    /* ASCII glyphs classified as marks cannot bypass IgnoreMarks filtering. */
    mulib_test_font_make_(bytes);mulib_sfnt_font_t view;mulib_typecore_slice_t table;
    assert(mulib_sfnt_open(&view,bytes,size)&&mulib_sfnt_table(&view,"GDEF",&table));
    mulib_test_font_u16_(bytes,(size_t)(table.data-bytes)+20,3);
    f=mulib_fontshape_open_memory(bytes,size,0,1000);assert(f);
    assert(mulib_fontshape_shape(f,"fi",2,NULL,&run)==-1&&!run.glyphs&&!run.count);
    mulib_fontshape_close(f);
    mulib_test_font_make_(bytes);assert(mulib_sfnt_open(&view,bytes,size)&&mulib_sfnt_table(&view,"hmtx",&table));
    mulib_test_font_u16_(bytes,(size_t)(table.data-bytes)+4,65535);
    f=mulib_fontshape_open_memory(bytes,size,0,INT_MAX);assert(f);
    assert(mulib_fontshape_shape(f,"f",1,NULL,&run)==-1&&!run.glyphs&&!run.count);
    mulib_fontshape_close(f);
    return 0;
}
