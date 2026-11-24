/** @file mulib-fontcmap.h
 * @brief Checked OpenType cmap formats 4, 6, 10, 12, 13 and variation sequences.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_FONTCMAP once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-fontcmap.md for scope and limitations.
 */
#ifndef MULIB_FONTCMAP_H
#define MULIB_FONTCMAP_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_fontcmap_map { mulib_typecore_slice_t format12; } mulib_fontcmap_map_t;
/* The legacy field name is retained; it now also holds other supported formats. */
int mulib_fontcmap_open_format12(mulib_fontcmap_map_t *out,const void *table,size_t length);
int mulib_fontcmap_open_subtable(mulib_fontcmap_map_t *out,const void *table,size_t length);
int mulib_fontcmap_open(mulib_fontcmap_map_t *out,const void *cmap,size_t length);
int mulib_fontcmap_glyph(const mulib_fontcmap_map_t *map,uint32_t scalar,uint32_t *glyph);
int mulib_fontcmap_variation(const mulib_fontcmap_map_t *map,const void *format14,size_t length,
                            uint32_t scalar,uint32_t selector,uint32_t *glyph);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FONTCMAP_H */
#if defined(MULIB_IMPL_FONTCMAP) && !defined(MULIB_FONTCMAP_H_IMPLEMENTED)
#define MULIB_FONTCMAP_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_fontcmap_scalar_(uint32_t c) { return c<=0x10ffffu&&(c<0xd800u||c>0xdfffu); }
static int mulib_fontcmap_header_(mulib_typecore_slice_t *s,uint16_t *fmt) {
    uint16_t small; uint32_t length;
    if(!mulib_typecore_read16be(*s,0,fmt))return 0;
    if(*fmt==4||*fmt==6) {
        if(!mulib_typecore_read16be(*s,2,&small))return 0;
        length=small;
    } else if(*fmt==10||*fmt==12||*fmt==13) {
        if(!mulib_typecore_read16be(*s,2,&small)||small||
           !mulib_typecore_read32be(*s,4,&length))return 0;
    } else return 0;
    if(length>s->size||length<(*fmt==4?16u:*fmt==6?10u:*fmt==10?20u:16u))return 0;
    s->size=length; return 1;
}
int mulib_fontcmap_open_subtable(mulib_fontcmap_map_t *out,const void *data,size_t size) {
    mulib_typecore_slice_t s={(const unsigned char*)data,size}; uint16_t fmt;
    if(!out||!mulib_fontcmap_header_(&s,&fmt))return 0;
    if(fmt==4) {
        uint16_t twice,pad,previous=0;
        if(!mulib_typecore_read16be(s,6,&twice)||!twice||(twice&1)||
           (size_t)(twice/2)>(s.size-16)/8)return 0;
        size_t count=twice/2, end=16+count*8;
        if(!mulib_typecore_read16be(s,14+count*2,&pad)||pad)return 0;
        for(size_t i=0;i<count;i++) {
            uint16_t first,last,offset; size_t at=16+count*6+i*2;
            if(!mulib_typecore_read16be(s,14+i*2,&last)||
               !mulib_typecore_read16be(s,16+count*2+i*2,&first)||
               !mulib_typecore_read16be(s,at,&offset)||first>last||(i&&first<=previous))return 0;
            if(offset) {
                size_t target=at+offset;
                if((offset&1)||target<end||target>s.size||
                   (size_t)(last-first)>((s.size-target)/2)||
                   s.size-target-(size_t)(last-first)*2<2)return 0;
            }
            previous=last;
        }
        if(previous!=0xffffu)return 0;
    } else if(fmt==6||fmt==10) {
        uint32_t first,count; uint16_t a,b; size_t header=fmt==6?10:20;
        if(fmt==6) {
            if(!mulib_typecore_read16be(s,6,&a)||!mulib_typecore_read16be(s,8,&b))return 0;
            first=a; count=b;
        } else if(!mulib_typecore_read32be(s,12,&first)||!mulib_typecore_read32be(s,16,&count))return 0;
        if(count>(s.size-header)/2||first>(fmt==6?0x10000u:0x110000u)||
           count>(fmt==6?0x10000u:0x110000u)-first)return 0;
    } else {
        uint32_t count,previous=0;
        if(!mulib_typecore_read32be(s,12,&count)||count>(s.size-16)/12)return 0;
        for(size_t i=0;i<count;i++) {
            uint32_t first,last,glyph; size_t at=16+i*12;
            if(!mulib_typecore_read32be(s,at,&first)||!mulib_typecore_read32be(s,at+4,&last)||
               !mulib_typecore_read32be(s,at+8,&glyph)||first>last||last>0x10ffffu||
               (i&&first<=previous)||(fmt==12&&glyph>UINT32_MAX-(last-first)))return 0;
            previous=last;
        }
    }
    out->format12=s; return 1;
}
int mulib_fontcmap_open_format12(mulib_fontcmap_map_t *out,const void *p,size_t n) {
    mulib_typecore_slice_t s={(const unsigned char*)p,n}; uint16_t format;
    return mulib_typecore_read16be(s,0,&format)&&format==12&&mulib_fontcmap_open_subtable(out,p,n);
}
int mulib_fontcmap_open(mulib_fontcmap_map_t *out,const void *p,size_t n) {
    mulib_typecore_slice_t s={(const unsigned char*)p,n}; uint16_t version,count;
    mulib_typecore_slice_t best={NULL,0}; unsigned score=0;
    if(!out||!mulib_typecore_read16be(s,0,&version)||version||
       !mulib_typecore_read16be(s,2,&count)||count>4096||count>(n-4)/8)return 0;
    for(size_t i=0;i<count;i++) {
        uint16_t platform,encoding,format; uint32_t offset; size_t at=4+i*8;
        if(!mulib_typecore_read16be(s,at,&platform)||!mulib_typecore_read16be(s,at+2,&encoding)||
           !mulib_typecore_read32be(s,at+4,&offset)||offset>n||n-offset<2)return 0;
        if(platform!=0&&(platform!=3||(encoding!=1&&encoding!=10)))continue;
        if(!mulib_typecore_read16be(s,offset,&format))return 0;
        unsigned rank=format==12?5:format==13?4:format==10?3:format==4?2:format==6?1:0;
        if(!rank)continue;
        if(offset<4+(size_t)count*8)return 0;
        rank=rank*2+(platform==0);
        if(rank>score){best=(mulib_typecore_slice_t){s.data+offset,n-offset};score=rank;}
    }
    if(!score)return 0;
    /* Validate the selected subtable once, not once per aliased encoding record. */
    return mulib_fontcmap_open_subtable(out,best.data,best.size);
}
int mulib_fontcmap_glyph(const mulib_fontcmap_map_t *map,uint32_t scalar,uint32_t *out) {
    uint16_t fmt; uint32_t value=0;
    if(!map||!out||!mulib_fontcmap_scalar_(scalar))return 0;
    mulib_typecore_slice_t s=map->format12;
    if(!mulib_fontcmap_header_(&s,&fmt))return 0;
    if(fmt==4) {
        uint16_t twice;
        if(!mulib_typecore_read16be(s,6,&twice)||!twice||(twice&1)||twice/2>(s.size-16)/8)return 0;
        size_t count=twice/2,lo=0,hi=count;
        while(lo<hi) {
            size_t mid=lo+(hi-lo)/2; uint16_t last;
            if(!mulib_typecore_read16be(s,14+mid*2,&last))return 0;
            if(scalar>last)lo=mid+1;else hi=mid;
        }
        if(lo<count) {
            uint16_t first,delta,offset,glyph; size_t at=16+count*6+lo*2;
            if(!mulib_typecore_read16be(s,16+count*2+lo*2,&first)||
               !mulib_typecore_read16be(s,16+count*4+lo*2,&delta)||
               !mulib_typecore_read16be(s,at,&offset))return 0;
            if(scalar>=first&&scalar<=0xffffu) {
                if(offset) {
                    size_t target=at+offset+(size_t)(scalar-first)*2;
                    if((offset&1)||target<16+count*8||!mulib_typecore_read16be(s,target,&glyph))return 0;
                    value=glyph?(uint16_t)(glyph+delta):0;
                } else value=(uint16_t)(scalar+delta);
            }
        }
    } else if(fmt==6||fmt==10) {
        uint32_t first,count; uint16_t a,b,glyph; size_t header=fmt==6?10:20;
        if(fmt==6) {
            if(!mulib_typecore_read16be(s,6,&a)||!mulib_typecore_read16be(s,8,&b))return 0;
            first=a; count=b;
        } else if(!mulib_typecore_read32be(s,12,&first)||!mulib_typecore_read32be(s,16,&count))return 0;
        if(count>(s.size-header)/2)return 0;
        if(scalar>=first&&scalar-first<count) {
            if(!mulib_typecore_read16be(s,header+(size_t)(scalar-first)*2,&glyph))return 0;
            value=glyph;
        }
    } else {
        uint32_t count;
        if(!mulib_typecore_read32be(s,12,&count)||count>(s.size-16)/12)return 0;
        size_t lo=0,hi=count;
        while(lo<hi) {
            size_t mid=lo+(hi-lo)/2,at=16+mid*12; uint32_t first,last,glyph;
            if(!mulib_typecore_read32be(s,at,&first)||!mulib_typecore_read32be(s,at+4,&last)||
               !mulib_typecore_read32be(s,at+8,&glyph))return 0;
            if(scalar<first)hi=mid;else if(scalar>last)lo=mid+1;
            else {if(fmt==12&&glyph>UINT32_MAX-(scalar-first))return 0;
                value=fmt==12?glyph+scalar-first:glyph;break;}
        }
    }
    *out=value; return 1;
}
static int mulib_fontcmap_u24_(mulib_typecore_slice_t s,size_t at,uint32_t *out) {
    if(!s.data||at>s.size||s.size-at<3)return 0;
    *out=((uint32_t)s.data[at]<<16)|((uint32_t)s.data[at+1]<<8)|s.data[at+2];return 1;
}
int mulib_fontcmap_variation(const mulib_fontcmap_map_t *map,const void *p,size_t n,
                            uint32_t scalar,uint32_t selector,uint32_t *out) {
    mulib_typecore_slice_t s={(const unsigned char*)p,n};
    uint16_t format; uint32_t length,count,previous=0,result=0;
    if(!map||!out||!mulib_fontcmap_scalar_(scalar)||
       !((selector>=0xfe00u&&selector<=0xfe0fu)||(selector>=0xe0100u&&selector<=0xe01efu))||
       !mulib_typecore_read16be(s,0,&format)||format!=14||
       !mulib_typecore_read32be(s,2,&length)||length<10||length>n||
       !mulib_typecore_read32be(s,6,&count)||count>256||count>(length-10)/11)return 0;
    s.size=length;size_t budget=length;
    for(size_t i=0;i<count;i++) {
        uint32_t vs,defaults,mappings; size_t at=10+i*11;
        if(!mulib_fontcmap_u24_(s,at,&vs)||vs<=previous||
           !((vs>=0xfe00u&&vs<=0xfe0fu)||(vs>=0xe0100u&&vs<=0xe01efu))||
           !mulib_typecore_read32be(s,at+3,&defaults)||!mulib_typecore_read32be(s,at+7,&mappings))return 0;
        previous=vs;
        for(unsigned kind=0;kind<2;kind++) {
            uint32_t offset=kind?mappings:defaults,entries,last=0;
            size_t stride=kind?5:4;
            if(!offset)continue;
            if(offset>s.size||offset<10+(size_t)count*11||!mulib_typecore_read32be(s,offset,&entries)||
               entries>(s.size-offset-4)/stride)return 0;
            if(entries>budget/stride)return 0;
            budget-=(size_t)entries*stride;
            for(size_t j=0;j<entries;j++) {
                uint32_t first,end; uint16_t glyph; size_t record=(size_t)offset+4+j*stride;
                if(!mulib_fontcmap_u24_(s,record,&first))return 0;
                end=kind?first:first+s.data[record+3];
                if(!mulib_fontcmap_scalar_(first)||!mulib_fontcmap_scalar_(end)||
                   (first<0xd800u&&end>=0xd800u)||(j&&first<=last))return 0;
                last=end;
                if(vs==selector&&scalar>=first&&scalar<=end) {
                    if(kind) {if(!mulib_typecore_read16be(s,record+3,&glyph))return 0;result=glyph;}
                    else if(!mulib_fontcmap_glyph(map,scalar,&result))return 0;
                }
            }
        }
    }
    *out=result; return 1;
}
#ifdef __cplusplus
}
#endif
#endif
