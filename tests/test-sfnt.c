/* Standalone API contract test for mulib-sfnt. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_SFNT
#include "mulib-sfnt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "fixtures/publishing-font.h"
int main(void){
    unsigned char f[28]={0,1,0,0,0,1};memcpy(f+12,"head",4);f[23]=28;
    mulib_sfnt_font_t font;assert(mulib_sfnt_open(&font,f,sizeof(f)));mulib_typecore_slice_t table;
    assert(mulib_sfnt_table(&font,"head",&table)&&table.size==0);
    unsigned char data[1024];size_t size=mulib_test_font_make_(data);uint16_t units=123;
    assert(mulib_sfnt_open(&font,data,size)&&mulib_sfnt_units_per_em(&font,&units)&&units==1000);
    assert(!mulib_sfnt_table(&font,"XXXX",&table));
    mulib_sfnt_font_t saved=font;assert(!mulib_sfnt_open(&font,NULL,size));assert(font.data.data==saved.data.data);
    assert(!mulib_sfnt_open(&font,data,100));assert(font.data.size==saved.data.size);
    assert(mulib_sfnt_table(&font,"head",&table));
    mulib_test_font_u16_(data,(size_t)(table.data-data)+18,0);units=123;
    assert(!mulib_sfnt_units_per_em(&font,&units)&&units==123);
    mulib_test_font_make_(data);mulib_test_font_u32_(data,12+16+8,140);
    assert(!mulib_sfnt_open(&font,data,size)); /* table overlap */
    mulib_test_font_make_(data);memcpy(data+28,"GDEF",4);assert(!mulib_sfnt_open(&font,data,size));
    mulib_test_font_make_(data);mulib_test_font_u32_(data,20,0xffffffffu);
    assert(!mulib_sfnt_open(&font,data,size));
    font.data.data=NULL;font.data.size=100;font.tables=1;
    assert(!mulib_sfnt_table(&font,"head",&table));
    unsigned char ttc[24]={0};memcpy(ttc,"ttcf",4);mulib_test_font_u32_(ttc,4,0x20000);
    mulib_test_font_u32_(ttc,8,1);mulib_test_font_u32_(ttc,12,16);
    mulib_sfnt_face_t face;assert(!mulib_sfnt_open_face(&face,ttc,sizeof(ttc),0));
    return 0;
}
