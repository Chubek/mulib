#ifndef MULIB_TEST_PUBLISHING_FONT_H
#define MULIB_TEST_PUBLISHING_FONT_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
static inline void mulib_test_font_u16_(unsigned char *p,size_t at,unsigned value) {
    p[at]=(unsigned char)(value>>8);p[at+1]=(unsigned char)value;
}
static inline void mulib_test_font_u32_(unsigned char *p,size_t at,uint32_t value) {
    p[at]=(unsigned char)(value>>24);p[at+1]=(unsigned char)(value>>16);
    p[at+2]=(unsigned char)(value>>8);p[at+3]=(unsigned char)value;
}
/* One default Latin feature, one lookup. GSUB fi->glyph 4; GPOS A/V -50. */
static inline size_t mulib_test_font_layout_(unsigned char *p,int positioning) {
    memset(p,0,80);mulib_test_font_u16_(p,0,1);
    mulib_test_font_u16_(p,4,10);mulib_test_font_u16_(p,6,30);mulib_test_font_u16_(p,8,44);
    mulib_test_font_u16_(p,10,1);memcpy(p+12,"latn",4);mulib_test_font_u16_(p,16,8);
    mulib_test_font_u16_(p,18,4);mulib_test_font_u16_(p,24,0xffff);
    mulib_test_font_u16_(p,26,1);mulib_test_font_u16_(p,30,1);
    memcpy(p+32,positioning?"kern":"liga",4);mulib_test_font_u16_(p,36,8);
    mulib_test_font_u16_(p,40,1);mulib_test_font_u16_(p,44,1);mulib_test_font_u16_(p,46,4);
    mulib_test_font_u16_(p,48,positioning?2:4);mulib_test_font_u16_(p,52,1);
    mulib_test_font_u16_(p,54,8);mulib_test_font_u16_(p,56,1);mulib_test_font_u16_(p,58,18);
    if(positioning) {
        mulib_test_font_u16_(p,60,4);mulib_test_font_u16_(p,64,1);mulib_test_font_u16_(p,66,12);
        mulib_test_font_u16_(p,68,1);mulib_test_font_u16_(p,70,5);mulib_test_font_u16_(p,72,65536-50);
    } else {
        mulib_test_font_u16_(p,60,1);mulib_test_font_u16_(p,62,8);mulib_test_font_u16_(p,64,1);
        mulib_test_font_u16_(p,66,4);mulib_test_font_u16_(p,68,4);mulib_test_font_u16_(p,70,2);
        mulib_test_font_u16_(p,72,2);
    }
    mulib_test_font_u16_(p,74,1);mulib_test_font_u16_(p,76,1);
    mulib_test_font_u16_(p,78,positioning?3:1);return 80;
}
static inline size_t mulib_test_font_make_(unsigned char p[1024]) {
    static const char *const tags[]={"GDEF","GPOS","GSUB","cmap","head","hhea","hmtx","maxp"};
    static const size_t lengths[]={30,80,80,76,54,36,24,6};
    memset(p,0,1024);mulib_test_font_u32_(p,0,0x10000);mulib_test_font_u16_(p,4,8);
    size_t offset=140;
    for(size_t i=0;i<8;i++) {
        unsigned char *t=p+offset;size_t entry=12+i*16;
        memcpy(p+entry,tags[i],4);mulib_test_font_u32_(p,entry+8,(uint32_t)offset);
        mulib_test_font_u32_(p,entry+12,(uint32_t)lengths[i]);
        if(i==0) {
            mulib_test_font_u16_(t,0,1);mulib_test_font_u16_(t,4,12);
            mulib_test_font_u16_(t,12,1);mulib_test_font_u16_(t,16,6);
            for(size_t j=0;j<6;j++)mulib_test_font_u16_(t,18+j*2,j==4?2:1);
        } else if(i==1||i==2)mulib_test_font_layout_(t,i==1);
        else if(i==3) {
            mulib_test_font_u16_(t,2,1);mulib_test_font_u16_(t,4,3);mulib_test_font_u16_(t,6,10);
            mulib_test_font_u32_(t,8,12);mulib_test_font_u16_(t,12,12);
            mulib_test_font_u32_(t,16,64);mulib_test_font_u32_(t,24,4);
            const uint32_t chars[]={65,86,102,105},glyphs[]={3,5,1,2};
            for(size_t j=0;j<4;j++) {
                mulib_test_font_u32_(t,28+j*12,chars[j]);mulib_test_font_u32_(t,32+j*12,chars[j]);
                mulib_test_font_u32_(t,36+j*12,glyphs[j]);
            }
        } else if(i==4)mulib_test_font_u16_(t,18,1000);
        else if(i==5) {
            mulib_test_font_u32_(t,0,0x10000);mulib_test_font_u16_(t,4,800);
            mulib_test_font_u16_(t,6,65536-200);mulib_test_font_u16_(t,8,100);
            mulib_test_font_u16_(t,34,6);
        } else if(i==6) {
            const unsigned advances[]={500,600,400,700,900,700};
            for(size_t j=0;j<6;j++)mulib_test_font_u16_(t,j*4,advances[j]);
        } else {mulib_test_font_u32_(t,0,0x5000);mulib_test_font_u16_(t,4,6);}
        offset=(offset+lengths[i]+3)&~(size_t)3;
    }
    return offset;
}
#endif
