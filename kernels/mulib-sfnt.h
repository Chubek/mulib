/** @file mulib-sfnt.h
 * @brief Checked SFNT and TTC face directories with overlap and range defense.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_SFNT once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-sfnt.md for scope and limitations.
 */
#ifndef MULIB_SFNT_H
#define MULIB_SFNT_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_sfnt_font { mulib_typecore_slice_t data;uint16_t tables; } mulib_sfnt_font_t;
/* A collection face retains absolute table offsets into the borrowed file. */
typedef struct mulib_sfnt_face { mulib_sfnt_font_t font; size_t directory; } mulib_sfnt_face_t;
int mulib_sfnt_open(mulib_sfnt_font_t *f,const void *data,size_t size);
int mulib_sfnt_table(const mulib_sfnt_font_t *f,const char tag[4],mulib_typecore_slice_t *out);
int mulib_sfnt_units_per_em(const mulib_sfnt_font_t *f,uint16_t *units);
int mulib_sfnt_open_face(mulib_sfnt_face_t *face,const void *data,size_t size,unsigned index);
int mulib_sfnt_face_table(const mulib_sfnt_face_t *face,const char tag[4],mulib_typecore_slice_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SFNT_H */
#if defined(MULIB_IMPL_SFNT) && !defined(MULIB_SFNT_H_IMPLEMENTED)
#define MULIB_SFNT_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_sfnt_directory_(mulib_typecore_slice_t s,size_t at,uint16_t *tables) {
    uint32_t magic, previous=0; uint16_t count;
    if(!s.data||at>s.size||s.size-at<12||
       !mulib_typecore_read32be(s,at,&magic)||
       (magic!=0x00010000u&&magic!=0x4f54544fu&&magic!=0x74727565u)||
       !mulib_typecore_read16be(s,at+4,&count)||count>4096||
       count>(s.size-at-12)/16)return 0;
    size_t end=at+12+(size_t)count*16;
    for(size_t i=0;i<count;i++) {
        uint32_t tag,off,len; size_t record=at+12+i*16;
        if(!mulib_typecore_read32be(s,record,&tag)||
           !mulib_typecore_read32be(s,record+8,&off)||
           !mulib_typecore_read32be(s,record+12,&len)||
           (i&&tag<=previous)||off>s.size||len>s.size-off||
           (len&&off<end&&(size_t)off+len>at))return 0;
        previous=tag;
        for(size_t j=0;j<i&&len;j++) {
            uint32_t other,length;
            if(!mulib_typecore_read32be(s,at+12+j*16+8,&other)||
               !mulib_typecore_read32be(s,at+12+j*16+12,&length))return 0;
            if(length&&off<(size_t)other+length&&other<(size_t)off+len)return 0;
        }
    }
    *tables=count; return 1;
}
static int mulib_sfnt_table_(const mulib_sfnt_font_t *f,size_t directory,
                             const char tag[4],mulib_typecore_slice_t *out) {
    if(!f||!tag||!out||!f->data.data||directory>f->data.size||
       f->data.size-directory<12||f->tables>(f->data.size-directory-12)/16)return 0;
    for(size_t i=0;i<f->tables;i++) {
        size_t at=directory+12+i*16; uint32_t off,len;
        if(memcmp(f->data.data+at,tag,4))continue;
        if(!mulib_typecore_read32be(f->data,at+8,&off)||
           !mulib_typecore_read32be(f->data,at+12,&len)||
           off>f->data.size||len>f->data.size-off)return 0;
        *out=(mulib_typecore_slice_t){f->data.data+off,len}; return 1;
    }
    return 0;
}
int mulib_sfnt_open(mulib_sfnt_font_t *f,const void *p,size_t n) {
    mulib_typecore_slice_t s={(const unsigned char*)p,n}; uint16_t count;
    if(!f||!mulib_sfnt_directory_(s,0,&count))return 0;
    f->data=s; f->tables=count; return 1;
}
int mulib_sfnt_table(const mulib_sfnt_font_t *f,const char tag[4],mulib_typecore_slice_t *out) {
    return mulib_sfnt_table_(f,0,tag,out);
}
int mulib_sfnt_units_per_em(const mulib_sfnt_font_t *f,uint16_t *out) {
    mulib_typecore_slice_t h; uint16_t value;
    if(!out||!mulib_sfnt_table(f,"head",&h)||!mulib_typecore_read16be(h,18,&value)||
       value<16||value>16384)return 0;
    *out=value; return 1;
}
int mulib_sfnt_open_face(mulib_sfnt_face_t *out,const void *p,size_t n,unsigned index) {
    mulib_typecore_slice_t s={(const unsigned char*)p,n};
    uint32_t magic,version,count,offset=0; uint16_t tables;
    if(!out||!mulib_typecore_read32be(s,0,&magic))return 0;
    if(magic==0x74746366u) {
        if(!mulib_typecore_read32be(s,4,&version)||(version!=0x10000u&&version!=0x20000u)||
           !mulib_typecore_read32be(s,8,&count)||!count||count>4096||
           count>(n-12)/4||index>=count||
           !mulib_typecore_read32be(s,12+(size_t)index*4,&offset)||
           offset<12+(size_t)count*4)return 0;
        if(version==0x20000u) {
            uint32_t tag,at,length; size_t dsig=12+(size_t)count*4;
            if(!mulib_typecore_read32be(s,dsig,&tag)||
               !mulib_typecore_read32be(s,dsig+4,&length)||
               !mulib_typecore_read32be(s,dsig+8,&at)||at>n||length>n-at||
               (tag&&tag!=0x44534947u)||offset<dsig+12)return 0;
        }
    } else if(index)return 0;
    if(offset>n||!mulib_sfnt_directory_(s,offset,&tables))return 0;
    out->font.data=s; out->font.tables=tables; out->directory=offset; return 1;
}
int mulib_sfnt_face_table(const mulib_sfnt_face_t *f,const char tag[4],mulib_typecore_slice_t *out) {
    return f&&mulib_sfnt_table_(&f->font,f->directory,tag,out);
}
#ifdef __cplusplus
}
#endif
#endif
