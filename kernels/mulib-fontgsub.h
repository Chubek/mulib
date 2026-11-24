/** @file mulib-fontgsub.h
 * @brief Checked GSUB views and bounded single/ligature substitution execution.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_FONTGSUB once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-fontgsub.md for scope and limitations.
 */
#ifndef MULIB_FONTGSUB_H
#define MULIB_FONTGSUB_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_fontgsub_view { mulib_typecore_slice_t bytes; } mulib_fontgsub_view_t;
int mulib_fontgsub_open(mulib_fontgsub_view_t *v, const void *bytes, size_t size);
int mulib_fontgsub_read_u16(const mulib_fontgsub_view_t *v, size_t offset, uint16_t *value);
int mulib_fontgsub_read_u32(const mulib_fontgsub_view_t *v, size_t offset, uint32_t *value);
int mulib_fontgsub_slice(const mulib_fontgsub_view_t *v, size_t offset, size_t length, mulib_typecore_slice_t *out);
/* Execute one lookup at the start of a caller-verified mark-free sequence.
 * Flags 0/IgnoreMarks; types 1/4 and type 7 extensions to those types.
 * No match consumes one unchanged glyph. No GDEF filtering is done here. */
int mulib_fontgsub_substitute(const mulib_fontgsub_view_t *v,unsigned lookup,
                             const uint32_t *glyphs,size_t count,uint32_t *replacement,size_t *consumed);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FONTGSUB_H */
#if defined(MULIB_IMPL_FONTGSUB) && !defined(MULIB_FONTGSUB_H_IMPLEMENTED)
#define MULIB_FONTGSUB_H_IMPLEMENTED
#include "detail/mulib-otlayout.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fontgsub_open(mulib_fontgsub_view_t *v,const void *bytes,size_t n){if(!v||(!bytes&&n))return 0;v->bytes=(mulib_typecore_slice_t){(const unsigned char*)bytes,n};return 1;}
int mulib_fontgsub_read_u16(const mulib_fontgsub_view_t *v,size_t i,uint16_t *o){return v&&mulib_typecore_read16be(v->bytes,i,o);}
int mulib_fontgsub_read_u32(const mulib_fontgsub_view_t *v,size_t i,uint32_t *o){return v&&mulib_typecore_read32be(v->bytes,i,o);}
int mulib_fontgsub_slice(const mulib_fontgsub_view_t *v,size_t at,size_t len,mulib_typecore_slice_t *o){if(!v||!o||(!v->bytes.data&&v->bytes.size)||at>v->bytes.size||len>v->bytes.size-at)return 0;*o=(mulib_typecore_slice_t){v->bytes.data?v->bytes.data+at:NULL,len};return 1;}
int mulib_fontgsub_substitute(const mulib_fontgsub_view_t *v,unsigned index,
                             const uint32_t *glyphs,size_t count,uint32_t *replacement,size_t *consumed) {
    uint16_t kind,subtables;size_t lookup;
    if(!v||!glyphs||!count||!replacement||!consumed||glyphs[0]>65535u||
       !mulib_otlayout_lookup_(v->bytes,index,&kind,&lookup,&subtables))return 0;
    mulib_typecore_slice_t s=v->bytes;
    uint32_t steps=65536u;
    if(kind!=1&&kind!=4&&kind!=7)return 0;
    uint32_t result=glyphs[0];size_t used=1;
    for(size_t i=0;i<subtables;i++) {
        if(!steps)return 0;
        steps--;
        size_t table,coverage,covered;uint16_t type=kind,format;
        if(!mulib_otlayout_offset_(s,lookup,6+i*2,&table))return 0;
        if(type==7) {
            uint32_t offset;
            if(s.size-table<8||!mulib_typecore_read16be(s,table,&format)||format!=1||
               !mulib_typecore_read16be(s,table+2,&type)||(type!=1&&type!=4)||
               !mulib_typecore_read32be(s,table+4,&offset)||offset<8||
               !mulib_otlayout_at_(s,table,offset,&table))return 0;
        }
        if(s.size-table<6||!mulib_typecore_read16be(s,table,&format)||
           !mulib_otlayout_offset_(s,table,2,&coverage)||
           !mulib_otlayout_coverage_(s,coverage,glyphs[0],&covered))return 0;
        if((type==1&&(format!=1&&format!=2))||(type==4&&format!=1))return 0;
        if(covered==SIZE_MAX)continue;
        if(type==1) {
            uint16_t value;
            if(format==1) {
                if(!mulib_typecore_read16be(s,table+4,&value))return 0;
                result=(uint16_t)(glyphs[0]+value);
            } else {
                uint16_t entries;
                if(!mulib_typecore_read16be(s,table+4,&entries)||entries>(s.size-table-6)/2||
                   covered>=entries||!mulib_typecore_read16be(s,table+6+covered*2,&value))return 0;
                result=value;
            }
            break;
        }
        uint16_t sets,ligatures;size_t set;
        if(!mulib_typecore_read16be(s,table+4,&sets)||sets>(s.size-table-6)/2||covered>=sets||
           !mulib_otlayout_offset_(s,table,6+covered*2,&set)||
           !mulib_typecore_read16be(s,set,&ligatures)||ligatures>(s.size-set-2)/2)return 0;
        for(size_t j=0;j<ligatures;j++) {
            if(!steps)return 0;
            steps--;
            size_t ligature;uint16_t glyph,components;
            if(!mulib_otlayout_offset_(s,set,2+j*2,&ligature)||s.size-ligature<4||
               !mulib_typecore_read16be(s,ligature,&glyph)||
               !mulib_typecore_read16be(s,ligature+2,&components)||components<2||
               (size_t)(components-1)>(s.size-ligature-4)/2)return 0;
            if(components>count)continue;
            size_t k=1;
            for(;k<components;k++) {
                if(!steps)return 0;
                steps--;
                uint16_t component;
                if(!mulib_typecore_read16be(s,ligature+4+(k-1)*2,&component))return 0;
                if(component!=glyphs[k])break;
            }
            if(k==components){result=glyph;used=components;break;}
        }
        if(used>1)break;
    }
    *replacement=result;*consumed=used;return 1;
}
#ifdef __cplusplus
}
#endif
#endif
