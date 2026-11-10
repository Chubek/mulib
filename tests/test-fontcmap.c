/* Standalone API contract test for mulib-fontcmap. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_FONTCMAP
#include "mulib-fontcmap.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "fixtures/publishing-font.h"
int main(void){
    unsigned char t[80]={0,12,0,0,0,0,0,28,0,0,0,0,0,0,0,1,0,0,0,65,0,0,0,65,0,0,0,7};
    mulib_fontcmap_map_t m;uint32_t g=0;
    assert(mulib_fontcmap_open_format12(&m,t,28));
    assert(mulib_fontcmap_glyph(&m,65,&g)&&g==7);assert(mulib_fontcmap_glyph(&m,66,&g)&&g==0);
    g=123;assert(!mulib_fontcmap_glyph(&m,0xd800,&g)&&g==123);
    mulib_test_font_u32_(t,20,67);mulib_test_font_u32_(t,24,UINT32_MAX);
    assert(!mulib_fontcmap_open_format12(&m,t,28));
    mulib_test_font_u16_(t,0,13);assert(mulib_fontcmap_open_subtable(&m,t,28));
    assert(mulib_fontcmap_glyph(&m,66,&g)&&g==UINT32_MAX);
    /* Format 6, then format 10 supplementary-plane trimmed array. */
    memset(t,0,sizeof(t));mulib_test_font_u16_(t,0,6);mulib_test_font_u16_(t,2,14);
    mulib_test_font_u16_(t,6,65);mulib_test_font_u16_(t,8,2);
    mulib_test_font_u16_(t,10,7);mulib_test_font_u16_(t,12,8);
    assert(mulib_fontcmap_open_subtable(&m,t,14));assert(mulib_fontcmap_glyph(&m,66,&g)&&g==8);
    assert(!mulib_fontcmap_open_format12(&m,t,14));
    memset(t,0,sizeof(t));mulib_test_font_u16_(t,0,10);mulib_test_font_u32_(t,4,24);
    mulib_test_font_u32_(t,12,0x1f600);mulib_test_font_u32_(t,16,2);
    mulib_test_font_u16_(t,20,11);mulib_test_font_u16_(t,22,12);
    assert(mulib_fontcmap_open_subtable(&m,t,24));assert(mulib_fontcmap_glyph(&m,0x1f601,&g)&&g==12);
    /* Format 4: delta segment and sentinel. */
    memset(t,0,sizeof(t));mulib_test_font_u16_(t,0,4);mulib_test_font_u16_(t,2,32);
    mulib_test_font_u16_(t,6,4);mulib_test_font_u16_(t,14,66);mulib_test_font_u16_(t,16,0xffff);
    mulib_test_font_u16_(t,20,65);mulib_test_font_u16_(t,22,0xffff);
    mulib_test_font_u16_(t,24,65536-58);mulib_test_font_u16_(t,26,1);
    assert(mulib_fontcmap_open_subtable(&m,t,32));assert(mulib_fontcmap_glyph(&m,65,&g)&&g==7);
    assert(mulib_fontcmap_glyph(&m,66,&g)&&g==8);assert(mulib_fontcmap_glyph(&m,64,&g)&&g==0);
    /* idRangeOffset is relative to its own word; glyph zero is not delta-shifted. */
    mulib_test_font_u16_(t,2,36);mulib_test_font_u16_(t,24,3);mulib_test_font_u16_(t,28,4);
    mulib_test_font_u16_(t,32,5);mulib_test_font_u16_(t,34,0);
    assert(mulib_fontcmap_open_subtable(&m,t,36));assert(mulib_fontcmap_glyph(&m,65,&g)&&g==8);
    assert(mulib_fontcmap_glyph(&m,66,&g)&&g==0);
    mulib_test_font_u16_(t,28,0xfffe);assert(!mulib_fontcmap_open_subtable(&m,t,36));
    mulib_test_font_u16_(t,28,4);
    unsigned char cmap[48]={0};mulib_test_font_u16_(cmap,2,1);mulib_test_font_u16_(cmap,4,3);
    mulib_test_font_u16_(cmap,6,1);mulib_test_font_u32_(cmap,8,12);memcpy(cmap+12,t,36);
    assert(mulib_fontcmap_open(&m,cmap,sizeof(cmap)));assert(mulib_fontcmap_glyph(&m,65,&g)&&g==8);
    for(size_t n=0;n<36;n++)assert(!mulib_fontcmap_open_subtable(&m,t,n));
    assert(mulib_fontcmap_open(&m,cmap,sizeof(cmap)));
    /* Format 14: FE0F default A, explicit B->42. */
    unsigned char variation[38]={0};mulib_test_font_u16_(variation,0,14);
    mulib_test_font_u32_(variation,2,38);mulib_test_font_u32_(variation,6,1);
    variation[11]=0xfe;variation[12]=0x0f;
    mulib_test_font_u32_(variation,13,21);mulib_test_font_u32_(variation,17,29);
    mulib_test_font_u32_(variation,21,1);variation[27]=65;
    mulib_test_font_u32_(variation,29,1);variation[35]=66;mulib_test_font_u16_(variation,36,42);
    assert(mulib_fontcmap_variation(&m,variation,sizeof(variation),65,0xfe0f,&g)&&g==8);
    assert(mulib_fontcmap_variation(&m,variation,sizeof(variation),66,0xfe0f,&g)&&g==42);
    assert(mulib_fontcmap_variation(&m,variation,sizeof(variation),67,0xfe0f,&g)&&g==0);
    assert(!mulib_fontcmap_variation(&m,variation,sizeof(variation),65,123,&g));
    for(size_t n=0;n<sizeof(variation);n++)assert(!mulib_fontcmap_variation(&m,variation,n,65,0xfe0f,&g));
    /* Many encoding records may legitimately share one selected subtable. */
    size_t directory=4u+4096u*8u;unsigned char *aliases=(unsigned char*)calloc(directory+36,1);assert(aliases);
    mulib_test_font_u16_(aliases,2,4096);
    for(size_t i=0;i<4096;i++) {
        mulib_test_font_u16_(aliases,4+i*8,3);mulib_test_font_u16_(aliases,6+i*8,1);
        mulib_test_font_u32_(aliases,8+i*8,(uint32_t)directory);
    }
    memcpy(aliases+directory,t,36);mulib_fontcmap_map_t shared;
    assert(mulib_fontcmap_open(&shared,aliases,directory+36)&&mulib_fontcmap_glyph(&shared,65,&g)&&g==8);
    mulib_test_font_u16_(aliases,2,4097);assert(!mulib_fontcmap_open(&shared,aliases,directory+36));
    free(aliases);
    return 0;
}
