#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_FONTGSUB
#define MULIB_IMPL_FONTGPOS
#define MULIB_IMPL_FONTGDEF
#include "mulib-fontgsub.h"
#include "mulib-fontgpos.h"
#include "mulib-fontgdef.h"
#include "fixtures/publishing-font.h"
#include <assert.h>

int main(void) {
    unsigned char layout[128]={0};mulib_test_font_layout_(layout,0);
    mulib_fontgsub_view_t sub;mulib_fontgpos_view_t pos;mulib_fontgdef_view_t def;
    uint32_t glyphs[]={1,2,5},replacement=99;size_t consumed=99;
    assert(mulib_fontgsub_open(&sub,layout,80));
    assert(mulib_fontgsub_substitute(&sub,0,glyphs,3,&replacement,&consumed)&&replacement==4&&consumed==2);
    assert(mulib_fontgsub_substitute(&sub,0,glyphs,1,&replacement,&consumed)&&replacement==1&&consumed==1);
    assert(!mulib_fontgsub_substitute(&sub,1,glyphs,3,&replacement,&consumed));
    assert(!mulib_fontgsub_substitute(&sub,0,NULL,3,&replacement,&consumed));
    for(size_t n=0;n<80;n++) {
        assert(mulib_fontgsub_open(&sub,layout,n));replacement=99;consumed=99;
        assert(!mulib_fontgsub_substitute(&sub,0,glyphs,3,&replacement,&consumed)&&replacement==99&&consumed==99);
    }
    memmove(layout+64,layout+56,24);mulib_test_font_u16_(layout,48,7);
    mulib_test_font_u16_(layout,56,1);mulib_test_font_u16_(layout,58,4);
    mulib_test_font_u32_(layout,60,8);assert(mulib_fontgsub_open(&sub,layout,88));
    assert(mulib_fontgsub_substitute(&sub,0,glyphs,3,&replacement,&consumed)&&replacement==4&&consumed==2);
    mulib_test_font_u16_(layout,58,7);
    assert(!mulib_fontgsub_substitute(&sub,0,glyphs,3,&replacement,&consumed));
    mulib_test_font_layout_(layout,0);mulib_test_font_u16_(layout,48,1);
    mulib_test_font_u16_(layout,58,6);mulib_test_font_u16_(layout,60,3);
    mulib_test_font_u16_(layout,62,1);mulib_test_font_u16_(layout,64,1);mulib_test_font_u16_(layout,66,1);
    assert(mulib_fontgsub_open(&sub,layout,80));
    assert(mulib_fontgsub_substitute(&sub,0,glyphs,3,&replacement,&consumed)&&replacement==4&&consumed==1);
    mulib_test_font_u16_(layout,56,2);mulib_test_font_u16_(layout,58,8);
    mulib_test_font_u16_(layout,60,1);mulib_test_font_u16_(layout,62,5);
    mulib_test_font_u16_(layout,64,1);mulib_test_font_u16_(layout,66,1);mulib_test_font_u16_(layout,68,1);
    assert(mulib_fontgsub_substitute(&sub,0,glyphs,3,&replacement,&consumed)&&replacement==5);
    mulib_test_font_u16_(layout,50,16);
    assert(!mulib_fontgsub_substitute(&sub,0,glyphs,3,&replacement,&consumed));

    /* Reused offsets cannot turn one ligature set into quadratic work. */
    size_t hostile_size=72u+65535u*2u;
    unsigned char *hostile=(unsigned char*)calloc(hostile_size,1);assert(hostile);
    mulib_test_font_layout_(hostile,0);mulib_test_font_u16_(hostile,58,8);
    mulib_test_font_u16_(hostile,62,14);mulib_test_font_u16_(hostile,64,1);
    mulib_test_font_u16_(hostile,66,1);mulib_test_font_u16_(hostile,68,1);
    mulib_test_font_u16_(hostile,70,65535);
    for(size_t i=0;i<65535;i++)mulib_test_font_u16_(hostile,72+i*2,4);
    uint32_t four[]={1,2,3,5};replacement=99;consumed=99;
    assert(mulib_fontgsub_open(&sub,hostile,hostile_size));
    assert(!mulib_fontgsub_substitute(&sub,0,four,4,&replacement,&consumed)&&replacement==99&&consumed==99);
    free(hostile);

    mulib_fontgpos_adjustment_t a={0},b={0};mulib_test_font_layout_(layout,1);
    assert(mulib_fontgpos_open(&pos,layout,80));
    assert(mulib_fontgpos_pair(&pos,0,3,5,&a,&b,&consumed)&&a.x_advance==-50&&b.x_advance==0&&consumed==1);
    assert(mulib_fontgpos_pair(&pos,0,3,2,&a,&b,&consumed)&&a.x_advance==0);
    for(size_t n=0;n<80;n++) {
        assert(mulib_fontgpos_open(&pos,layout,n));a.x_advance=99;b.x_advance=99;consumed=99;
        assert(!mulib_fontgpos_pair(&pos,0,3,5,&a,&b,&consumed)&&a.x_advance==99&&b.x_advance==99&&consumed==99);
    }
    /* A nonempty second ValueRecord advances pair traversal by two glyphs. */
    mulib_test_font_u16_(layout,58,20);mulib_test_font_u16_(layout,62,1);
    mulib_test_font_u16_(layout,74,10);mulib_test_font_u16_(layout,76,1);
    mulib_test_font_u16_(layout,78,1);mulib_test_font_u16_(layout,80,3);
    assert(mulib_fontgpos_open(&pos,layout,82));
    assert(mulib_fontgpos_pair(&pos,0,3,5,&a,&b,&consumed)&&a.x_advance==-50&&b.x_placement==10&&consumed==2);
    /* Class pair positioning, two classes per side. */
    mulib_test_font_layout_(layout,1);
    mulib_test_font_u16_(layout,56,2);mulib_test_font_u16_(layout,58,24);
    mulib_test_font_u16_(layout,64,30);mulib_test_font_u16_(layout,66,38);
    mulib_test_font_u16_(layout,68,2);mulib_test_font_u16_(layout,70,2);
    memset(layout+72,0,56);mulib_test_font_u16_(layout,78,65536-80);
    mulib_test_font_u16_(layout,80,1);mulib_test_font_u16_(layout,82,1);mulib_test_font_u16_(layout,84,3);
    mulib_test_font_u16_(layout,86,1);mulib_test_font_u16_(layout,88,3);
    mulib_test_font_u16_(layout,90,1);mulib_test_font_u16_(layout,92,1);
    mulib_test_font_u16_(layout,94,1);mulib_test_font_u16_(layout,96,5);
    mulib_test_font_u16_(layout,98,1);mulib_test_font_u16_(layout,100,1);
    assert(mulib_fontgpos_open(&pos,layout,102));
    assert(mulib_fontgpos_pair(&pos,0,3,5,&a,&b,&consumed)&&a.x_advance==-80);
    memmove(layout+64,layout+56,46);mulib_test_font_u16_(layout,48,9);
    mulib_test_font_u16_(layout,56,1);mulib_test_font_u16_(layout,58,2);mulib_test_font_u32_(layout,60,8);
    assert(mulib_fontgpos_open(&pos,layout,110));
    assert(mulib_fontgpos_pair(&pos,0,3,5,&a,&b,&consumed)&&a.x_advance==-80);
    mulib_test_font_u16_(layout,58,9);assert(!mulib_fontgpos_pair(&pos,0,3,5,&a,&b,&consumed));
    mulib_test_font_layout_(layout,1);mulib_test_font_u16_(layout,60,0x10);
    assert(mulib_fontgpos_open(&pos,layout,80));
    assert(!mulib_fontgpos_pair(&pos,0,3,5,&a,&b,&consumed)); /* device adjustments unsupported */

    memset(layout,0,sizeof(layout));mulib_test_font_u16_(layout,0,1);mulib_test_font_u16_(layout,4,12);
    mulib_test_font_u16_(layout,12,1);mulib_test_font_u16_(layout,14,4);
    mulib_test_font_u16_(layout,16,1);mulib_test_font_u16_(layout,18,2);
    uint16_t kind=99;assert(mulib_fontgdef_open(&def,layout,20));
    assert(mulib_fontgdef_glyph_class(&def,4,&kind)&&kind==2);
    assert(mulib_fontgdef_glyph_class(&def,3,&kind)&&kind==0);
    mulib_test_font_u16_(layout,18,5);kind=99;
    assert(!mulib_fontgdef_glyph_class(&def,4,&kind)&&kind==99);
    assert(!mulib_fontgdef_glyph_class(&def,65536,&kind));
    mulib_typecore_slice_t empty;
    assert(mulib_fontgsub_open(&sub,NULL,0)&&mulib_fontgsub_slice(&sub,0,0,&empty)&&!empty.data);
    assert(mulib_fontgpos_open(&pos,NULL,0)&&mulib_fontgpos_slice(&pos,0,0,&empty)&&!empty.data);
    assert(mulib_fontgdef_open(&def,NULL,0)&&mulib_fontgdef_slice(&def,0,0,&empty)&&!empty.data);
    return 0;
}
