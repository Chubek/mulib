/** @file mulib-fontgdef.h
 * @brief Checked GDEF views and glyph class definition lookup.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_FONTGDEF once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-fontgdef.md for scope and limitations.
 */
#ifndef MULIB_FONTGDEF_H
#define MULIB_FONTGDEF_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_fontgdef_view { mulib_typecore_slice_t bytes; } mulib_fontgdef_view_t;
int mulib_fontgdef_open(mulib_fontgdef_view_t *v, const void *bytes, size_t size);
int mulib_fontgdef_read_u16(const mulib_fontgdef_view_t *v, size_t offset, uint16_t *value);
int mulib_fontgdef_read_u32(const mulib_fontgdef_view_t *v, size_t offset, uint32_t *value);
int mulib_fontgdef_slice(const mulib_fontgdef_view_t *v, size_t offset, size_t length, mulib_typecore_slice_t *out);
int mulib_fontgdef_glyph_class(const mulib_fontgdef_view_t *v,uint32_t glyph,uint16_t *glyph_class);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FONTGDEF_H */
#if defined(MULIB_IMPL_FONTGDEF) && !defined(MULIB_FONTGDEF_H_IMPLEMENTED)
#define MULIB_FONTGDEF_H_IMPLEMENTED
#include "detail/mulib-otlayout.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fontgdef_open(mulib_fontgdef_view_t *v,const void *bytes,size_t n){if(!v||(!bytes&&n))return 0;v->bytes=(mulib_typecore_slice_t){(const unsigned char*)bytes,n};return 1;}
int mulib_fontgdef_read_u16(const mulib_fontgdef_view_t *v,size_t i,uint16_t *o){return v&&mulib_typecore_read16be(v->bytes,i,o);}
int mulib_fontgdef_read_u32(const mulib_fontgdef_view_t *v,size_t i,uint32_t *o){return v&&mulib_typecore_read32be(v->bytes,i,o);}
int mulib_fontgdef_slice(const mulib_fontgdef_view_t *v,size_t at,size_t len,mulib_typecore_slice_t *o){if(!v||!o||(!v->bytes.data&&v->bytes.size)||at>v->bytes.size||len>v->bytes.size-at)return 0;*o=(mulib_typecore_slice_t){v->bytes.data?v->bytes.data+at:NULL,len};return 1;}
int mulib_fontgdef_glyph_class(const mulib_fontgdef_view_t *v,uint32_t glyph,uint16_t *out) {
    uint16_t major,minor,offset,value=0;
    if(!v||!out||glyph>65535u||v->bytes.size<12||
       !mulib_typecore_read16be(v->bytes,0,&major)||major!=1||
       !mulib_typecore_read16be(v->bytes,2,&minor)||minor>3||
       !mulib_typecore_read16be(v->bytes,4,&offset))return 0;
    if(offset&&(!mulib_otlayout_class_(v->bytes,offset,glyph,&value)||value>4))return 0;
    *out=value;return 1;
}
#ifdef __cplusplus
}
#endif
#endif
