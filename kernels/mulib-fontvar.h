/** @file mulib-fontvar.h
 * @brief Bounds-checked font and unicode fontvar structures binary inspection and record access.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_FONTVAR once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-fontvar.md for scope and limitations.
 */
#ifndef MULIB_FONTVAR_H
#define MULIB_FONTVAR_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_fontvar_view { mulib_typecore_slice_t bytes; } mulib_fontvar_view_t;
int mulib_fontvar_open(mulib_fontvar_view_t *v, const void *bytes, size_t size);
int mulib_fontvar_read_u16(const mulib_fontvar_view_t *v, size_t offset, uint16_t *value);
int mulib_fontvar_read_u32(const mulib_fontvar_view_t *v, size_t offset, uint32_t *value);
int mulib_fontvar_slice(const mulib_fontvar_view_t *v, size_t offset, size_t length, mulib_typecore_slice_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FONTVAR_H */
#if defined(MULIB_IMPL_FONTVAR) && !defined(MULIB_FONTVAR_H_IMPLEMENTED)
#define MULIB_FONTVAR_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fontvar_open(mulib_fontvar_view_t *v,const void *bytes,size_t n){if(!v||(!bytes&&n))return 0;v->bytes=(mulib_typecore_slice_t){(const unsigned char*)bytes,n};return 1;}
int mulib_fontvar_read_u16(const mulib_fontvar_view_t *v,size_t i,uint16_t *o){return v&&mulib_typecore_read16be(v->bytes,i,o);}
int mulib_fontvar_read_u32(const mulib_fontvar_view_t *v,size_t i,uint32_t *o){return v&&mulib_typecore_read32be(v->bytes,i,o);}
int mulib_fontvar_slice(const mulib_fontvar_view_t *v,size_t at,size_t len,mulib_typecore_slice_t *o){if(!v||!o||at>v->bytes.size||len>v->bytes.size-at)return 0;*o=(mulib_typecore_slice_t){v->bytes.data+at,len};return 1;}
#ifdef __cplusplus
}
#endif
#endif
