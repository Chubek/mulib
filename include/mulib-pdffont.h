/** @file mulib-pdffont.h
 * @brief Bounds-checked output pdffont structures binary inspection and record access.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_PDFFONT once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-pdffont.md for scope and limitations.
 */
#ifndef MULIB_PDFFONT_H
#define MULIB_PDFFONT_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_pdffont_view { mulib_typecore_slice_t bytes; } mulib_pdffont_view_t;
int mulib_pdffont_open(mulib_pdffont_view_t *v, const void *bytes, size_t size);
int mulib_pdffont_read_u16(const mulib_pdffont_view_t *v, size_t offset, uint16_t *value);
int mulib_pdffont_read_u32(const mulib_pdffont_view_t *v, size_t offset, uint32_t *value);
int mulib_pdffont_slice(const mulib_pdffont_view_t *v, size_t offset, size_t length, mulib_typecore_slice_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PDFFONT_H */
#if defined(MULIB_IMPL_PDFFONT) && !defined(MULIB_PDFFONT_H_IMPLEMENTED)
#define MULIB_PDFFONT_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pdffont_open(mulib_pdffont_view_t *v,const void *bytes,size_t n){if(!v||(!bytes&&n))return 0;v->bytes=(mulib_typecore_slice_t){(const unsigned char*)bytes,n};return 1;}
int mulib_pdffont_read_u16(const mulib_pdffont_view_t *v,size_t i,uint16_t *o){return v&&mulib_typecore_read16be(v->bytes,i,o);}
int mulib_pdffont_read_u32(const mulib_pdffont_view_t *v,size_t i,uint32_t *o){return v&&mulib_typecore_read32be(v->bytes,i,o);}
int mulib_pdffont_slice(const mulib_pdffont_view_t *v,size_t at,size_t len,mulib_typecore_slice_t *o){if(!v||!o||at>v->bytes.size||len>v->bytes.size-at)return 0;*o=(mulib_typecore_slice_t){v->bytes.data+at,len};return 1;}
#ifdef __cplusplus
}
#endif
#endif
