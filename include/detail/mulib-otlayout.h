/* Internal checked OpenType Layout primitives shared by GSUB/GPOS execution. */
#ifndef MULIB_OTLAYOUT_DETAIL_H
#define MULIB_OTLAYOUT_DETAIL_H
#include "../mulib-typecore.h"

static inline int mulib_otlayout_at_(mulib_typecore_slice_t s,size_t base,size_t relative,size_t *out) {
    if(base>s.size||relative>s.size-base)return 0;
    *out=base+relative;return 1;
}
static inline int mulib_otlayout_offset_(mulib_typecore_slice_t s,size_t base,size_t field,size_t *out) {
    size_t at;uint16_t value;
    return mulib_otlayout_at_(s,base,field,&at)&&mulib_typecore_read16be(s,at,&value)&&
           value&&mulib_otlayout_at_(s,base,value,out);
}
static inline int mulib_otlayout_coverage_(mulib_typecore_slice_t s,size_t at,uint32_t glyph,size_t *index) {
    uint16_t format,count;
    if(at>s.size||s.size-at<4||!mulib_typecore_read16be(s,at,&format)||
       !mulib_typecore_read16be(s,at+2,&count)||(format!=1&&format!=2)||
       count>(s.size-at-4)/(format==1?2:6))return 0;
    size_t lo=0,hi=count;*index=SIZE_MAX;
    while(lo<hi) {
        size_t mid=lo+(hi-lo)/2,record=at+4+mid*(format==1?2:6);
        uint16_t first,last,offset=0;
        if(!mulib_typecore_read16be(s,record,&first))return 0;
        last=first;
        if(format==2&&(!mulib_typecore_read16be(s,record+2,&last)||
                      !mulib_typecore_read16be(s,record+4,&offset)||first>last))return 0;
        if(glyph<first)hi=mid;else if(glyph>last)lo=mid+1;
        else {*index=format==1?mid:(size_t)offset+glyph-first;return 1;}
    }
    return 1;
}
static inline int mulib_otlayout_class_(mulib_typecore_slice_t s,size_t at,uint32_t glyph,uint16_t *value) {
    uint16_t format,count,first;
    if(at>s.size||s.size-at<4||!mulib_typecore_read16be(s,at,&format))return 0;
    *value=0;
    if(format==1) {
        if(s.size-at<6||!mulib_typecore_read16be(s,at+2,&first)||
           !mulib_typecore_read16be(s,at+4,&count)||count>(s.size-at-6)/2||
           count>65536u-(uint32_t)first)return 0;
        return glyph<first||glyph-first>=count||
               mulib_typecore_read16be(s,at+6+(size_t)(glyph-first)*2,value);
    }
    if(format!=2||!mulib_typecore_read16be(s,at+2,&count)||count>(s.size-at-4)/6)return 0;
    size_t lo=0,hi=count;
    while(lo<hi) {
        size_t mid=lo+(hi-lo)/2,record=at+4+mid*6;uint16_t last;
        if(!mulib_typecore_read16be(s,record,&first)||
           !mulib_typecore_read16be(s,record+2,&last)||first>last)return 0;
        if(glyph<first)hi=mid;else if(glyph>last)lo=mid+1;
        else return mulib_typecore_read16be(s,record+4,value);
    }
    return 1;
}
static inline int mulib_otlayout_lookup_(mulib_typecore_slice_t s,unsigned index,
                                        uint16_t *kind,size_t *lookup,uint16_t *subtables) {
    uint16_t major,minor,count,flags;size_t list;
    if(!mulib_typecore_read16be(s,0,&major)||major!=1||!mulib_typecore_read16be(s,2,&minor)||minor>1||
       !mulib_otlayout_offset_(s,0,8,&list)||!mulib_typecore_read16be(s,list,&count)||
       count>(s.size-list-2)/2||index>=count||
       !mulib_otlayout_offset_(s,list,2+(size_t)index*2,lookup)||
       s.size-*lookup<6||!mulib_typecore_read16be(s,*lookup,kind)||
       !mulib_typecore_read16be(s,*lookup+2,&flags)||(flags&~8u)||
       !mulib_typecore_read16be(s,*lookup+4,subtables)||
       *subtables>(s.size-*lookup-6)/2)return 0;
    return 1;
}
static inline int mulib_otlayout_feature_(mulib_typecore_slice_t s,uint32_t script,uint32_t feature,
                                         mulib_typecore_slice_t *indices) {
    uint16_t major,minor,count;size_t scripts,features,script_at=0,fallback=0,language;
    *indices=(mulib_typecore_slice_t){NULL,0};
    if(!s.size)return 1;
    if(!mulib_typecore_read16be(s,0,&major)||major!=1||!mulib_typecore_read16be(s,2,&minor)||minor>1||
       !mulib_otlayout_offset_(s,0,4,&scripts)||!mulib_otlayout_offset_(s,0,6,&features)||
       !mulib_typecore_read16be(s,scripts,&count)||count>(s.size-scripts-2)/6)return 0;
    for(size_t i=0;i<count;i++) {
        uint32_t tag;size_t at=scripts+2+i*6,target;
        if(!mulib_typecore_read32be(s,at,&tag)||!mulib_otlayout_offset_(s,scripts,2+i*6+4,&target))return 0;
        if(tag==script)script_at=target;
        if(tag==0x44464c54u)fallback=target;
    }
    if(!script_at)script_at=fallback;
    if(!script_at)return 1;
    uint16_t default_offset,required,order,available;
    if(!mulib_typecore_read16be(s,script_at,&default_offset))return 0;
    if(!default_offset)return 1;
    if(!mulib_otlayout_at_(s,script_at,default_offset,&language)||s.size-language<6||
       !mulib_typecore_read16be(s,language,&order)||order||
       !mulib_typecore_read16be(s,language+2,&required)||
       !mulib_typecore_read16be(s,language+4,&available)||available>(s.size-language-6)/2||
       !mulib_typecore_read16be(s,features,&count)||count>(s.size-features-2)/6)return 0;
    for(size_t i=0;i<(size_t)available+(required!=0xffffu);i++) {
        uint16_t index=required;size_t at;uint32_t tag;
        if(required==0xffffu||i) {
            size_t j=i-(required!=0xffffu);
            if(!mulib_typecore_read16be(s,language+6+j*2,&index))return 0;
        }
        if(index>=count)return 0;
        at=features+2+(size_t)index*6;
        if(!mulib_typecore_read32be(s,at,&tag))return 0;
        if(tag==feature) {
            size_t table;uint16_t lookups,parameters;
            if(!mulib_otlayout_offset_(s,features,2+(size_t)index*6+4,&table)||s.size-table<4||
               !mulib_typecore_read16be(s,table,&parameters)||parameters||
               !mulib_typecore_read16be(s,table+2,&lookups)||lookups>(s.size-table-4)/2)return 0;
            *indices=(mulib_typecore_slice_t){s.data+table+4,(size_t)lookups*2};return 1;
        }
    }
    return 1;
}
#endif
