/** @file mulib-fontmetrics.h
 * @brief HMetrics width lookup from caller-extracted hmtx table and hhea count.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_FONTMETRICS once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-fontmetrics.md for scope and limitations.
 */
#ifndef MULIB_FONTMETRICS_H
#define MULIB_FONTMETRICS_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fontmetrics_advance(const void *hmtx,size_t bytes,uint16_t long_metrics,uint16_t glyph,uint16_t *advance,uint16_t *lsb);
int mulib_fontmetrics_signed(const void *hmtx,size_t bytes,uint16_t long_metrics,uint16_t glyph,uint16_t *advance,int16_t *lsb);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FONTMETRICS_H */
#if defined(MULIB_IMPL_FONTMETRICS) && !defined(MULIB_FONTMETRICS_H_IMPLEMENTED)
#define MULIB_FONTMETRICS_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fontmetrics_advance(const void *p,size_t n,uint16_t longs,uint16_t glyph,uint16_t *advance,uint16_t *lsb){if(!p||!longs||!advance||!lsb||n<(size_t)longs*4)return 0;mulib_typecore_slice_t s={(const unsigned char*)p,n};uint16_t a,b;size_t at=(size_t)(glyph<longs?glyph:longs-1)*4;if(!mulib_typecore_read16be(s,at,&a))return 0;at=glyph<longs?at+2:(size_t)longs*4+(size_t)(glyph-longs)*2;if(!mulib_typecore_read16be(s,at,&b))return 0;*advance=a;*lsb=b;return 1;}
int mulib_fontmetrics_signed(const void *p,size_t n,uint16_t longs,uint16_t glyph,uint16_t *advance,int16_t *lsb){uint16_t a,b;if(!advance||!lsb||!mulib_fontmetrics_advance(p,n,longs,glyph,&a,&b))return 0;*advance=a;*lsb=(int16_t)(b<=INT16_MAX?(int32_t)b:(int32_t)b-65536);return 1;}
#ifdef __cplusplus
}
#endif
#endif
