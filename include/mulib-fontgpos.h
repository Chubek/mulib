/** @file mulib-fontgpos.h
 * @brief Checked GPOS views and explicit/class pair positioning execution.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_FONTGPOS once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-fontgpos.md for scope and limitations.
 */
#ifndef MULIB_FONTGPOS_H
#define MULIB_FONTGPOS_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_fontgpos_view { mulib_typecore_slice_t bytes; } mulib_fontgpos_view_t;
int mulib_fontgpos_open(mulib_fontgpos_view_t *v, const void *bytes, size_t size);
int mulib_fontgpos_read_u16(const mulib_fontgpos_view_t *v, size_t offset, uint16_t *value);
int mulib_fontgpos_read_u32(const mulib_fontgpos_view_t *v, size_t offset, uint32_t *value);
int mulib_fontgpos_slice(const mulib_fontgpos_view_t *v, size_t offset, size_t length, mulib_typecore_slice_t *out);
typedef struct mulib_fontgpos_adjustment { int32_t x_placement,y_placement,x_advance,y_advance; } mulib_fontgpos_adjustment_t;
/* Pair positioning formats 1/2 (lookup 2), or lookup 9 extension. Flags
 * 0/IgnoreMarks, caller-verified mark-free pair. Scalar ValueRecords only;
 * device/variation adjustments are not interpreted. consumed is 1 or 2. */
int mulib_fontgpos_pair(const mulib_fontgpos_view_t *v,unsigned lookup,uint32_t left,uint32_t right,
                       mulib_fontgpos_adjustment_t *first,mulib_fontgpos_adjustment_t *second,size_t *consumed);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FONTGPOS_H */
#if defined(MULIB_IMPL_FONTGPOS) && !defined(MULIB_FONTGPOS_H_IMPLEMENTED)
#define MULIB_FONTGPOS_H_IMPLEMENTED
#include "detail/mulib-otlayout.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fontgpos_open(mulib_fontgpos_view_t *v,const void *bytes,size_t n){if(!v||(!bytes&&n))return 0;v->bytes=(mulib_typecore_slice_t){(const unsigned char*)bytes,n};return 1;}
int mulib_fontgpos_read_u16(const mulib_fontgpos_view_t *v,size_t i,uint16_t *o){return v&&mulib_typecore_read16be(v->bytes,i,o);}
int mulib_fontgpos_read_u32(const mulib_fontgpos_view_t *v,size_t i,uint32_t *o){return v&&mulib_typecore_read32be(v->bytes,i,o);}
int mulib_fontgpos_slice(const mulib_fontgpos_view_t *v,size_t at,size_t len,mulib_typecore_slice_t *o){if(!v||!o||(!v->bytes.data&&v->bytes.size)||at>v->bytes.size||len>v->bytes.size-at)return 0;*o=(mulib_typecore_slice_t){v->bytes.data?v->bytes.data+at:NULL,len};return 1;}
static int mulib_fontgpos_value_(mulib_typecore_slice_t s,size_t at,uint16_t format,
                                mulib_fontgpos_adjustment_t *out,size_t *bytes) {
    if(format&0xfff0u)return 0;
    int32_t values[4]={0};size_t n=0;
    for(unsigned i=0;i<4;i++)if(format&(1u<<i)) {
        uint16_t value;
        if(!mulib_typecore_read16be(s,at+n,&value))return 0;
        values[i]=value<=INT16_MAX?(int32_t)value:(int32_t)value-65536;n+=2;
    }
    *out=(mulib_fontgpos_adjustment_t){values[0],values[1],values[2],values[3]};*bytes=n;return 1;
}
int mulib_fontgpos_pair(const mulib_fontgpos_view_t *v,unsigned index,uint32_t left,uint32_t right,
                       mulib_fontgpos_adjustment_t *first,mulib_fontgpos_adjustment_t *second,size_t *consumed) {
    uint16_t kind,subtables;size_t lookup;
    size_t skip=1;
    mulib_fontgpos_adjustment_t a={0},b={0};
    if(!v||!first||!second||!consumed||left>65535u||right>65535u||
       !mulib_otlayout_lookup_(v->bytes,index,&kind,&lookup,&subtables)||(kind!=2&&kind!=9))return 0;
    mulib_typecore_slice_t s=v->bytes;
    for(size_t i=0;i<subtables;i++) {
        size_t table,coverage,covered;uint16_t type=kind,format,v1,v2;
        if(!mulib_otlayout_offset_(s,lookup,6+i*2,&table))return 0;
        if(type==9) {
            uint32_t offset;
            if(s.size-table<8||!mulib_typecore_read16be(s,table,&format)||format!=1||
               !mulib_typecore_read16be(s,table+2,&type)||type!=2||
               !mulib_typecore_read32be(s,table+4,&offset)||offset<8||
               !mulib_otlayout_at_(s,table,offset,&table))return 0;
        }
        if(s.size-table<10||!mulib_typecore_read16be(s,table,&format)||(format!=1&&format!=2)||
           !mulib_otlayout_offset_(s,table,2,&coverage)||
           !mulib_typecore_read16be(s,table+4,&v1)||!mulib_typecore_read16be(s,table+6,&v2)||
           (v1&0xfff0u)||(v2&0xfff0u)||!mulib_otlayout_coverage_(s,coverage,left,&covered))return 0;
        if(covered==SIZE_MAX)continue;
        size_t bytes1=0,bytes2=0;
        for(unsigned bit=0;bit<4;bit++){if(v1&(1u<<bit))bytes1+=2;if(v2&(1u<<bit))bytes2+=2;}
        size_t record=0;
        if(format==1) {
            uint16_t sets,pairs;size_t set;
            if(!mulib_typecore_read16be(s,table+8,&sets)||sets>(s.size-table-10)/2||covered>=sets||
               !mulib_otlayout_offset_(s,table,10+covered*2,&set)||
               !mulib_typecore_read16be(s,set,&pairs)||pairs>(s.size-set-2)/(2+bytes1+bytes2))return 0;
            size_t lo=0,hi=pairs;
            while(lo<hi) {
                size_t mid=lo+(hi-lo)/2,at=set+2+mid*(2+bytes1+bytes2);uint16_t glyph;
                if(!mulib_typecore_read16be(s,at,&glyph))return 0;
                if(right<glyph)hi=mid;else if(right>glyph)lo=mid+1;else {record=at+2;break;}
            }
            if(!record)continue;
        } else {
            size_t class1,class2;uint16_t c1,c2,n1,n2;
            if(s.size-table<16||!mulib_otlayout_offset_(s,table,8,&class1)||
               !mulib_otlayout_offset_(s,table,10,&class2)||
               !mulib_typecore_read16be(s,table+12,&n1)||!n1||
               !mulib_typecore_read16be(s,table+14,&n2)||!n2||
               !mulib_otlayout_class_(s,class1,left,&c1)||!mulib_otlayout_class_(s,class2,right,&c2)||
               c1>=n1||c2>=n2)return 0;
            size_t stride=bytes1+bytes2;
            if(stride&&((size_t)n1>(s.size-table-16)/stride/n2))return 0;
            record=table+16+((size_t)c1*n2+c2)*stride;
        }
        size_t used;
        if(!mulib_fontgpos_value_(s,record,v1,&a,&used)||
           !mulib_fontgpos_value_(s,record+used,v2,&b,&used))return 0;
        skip=v2?2:1;
        break;
    }
    *first=a;*second=b;*consumed=skip;return 1;
}
#ifdef __cplusplus
}
#endif
#endif
