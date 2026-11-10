/** @file mulib-dvi.h
 * @brief DVI ID=2 file writer for empty pages and rectangular rules.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_DVI once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-dvi.md for scope and limitations.
 */
#ifndef MULIB_DVI_H
#define MULIB_DVI_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_dvi_write_rules(const int32_t *heights,const int32_t *widths,size_t count,mulib_typecore_buf_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DVI_H */
#if defined(MULIB_IMPL_DVI) && !defined(MULIB_DVI_H_IMPLEMENTED)
#define MULIB_DVI_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_dvi_u32_(mulib_typecore_buf_t *b,uint32_t v){return mulib_typecore_u32be(b,v);}
int mulib_dvi_write_rules(const int32_t *h,const int32_t *w,size_t n,mulib_typecore_buf_t *out){if(!out||n>65535||(!h&&n)||(!w&&n))return 0;const uint32_t num=25400000u,den=473628672u,mag=1000u;
 mulib_typecore_buf_t b={0};if(!mulib_typecore_byte(&b,247)||!mulib_typecore_byte(&b,2)||!mulib_dvi_u32_(&b,num)||!mulib_dvi_u32_(&b,den)||!mulib_dvi_u32_(&b,mag)||!mulib_typecore_byte(&b,0))goto fail;
 uint32_t prev=0xffffffffu,max_v=0,max_h=0;for(size_t i=0;i<n;i++){if(h[i]<0||w[i]<0||b.size>UINT32_MAX)goto fail;if((uint32_t)h[i]>max_v)max_v=(uint32_t)h[i];if((uint32_t)w[i]>max_h)max_h=(uint32_t)w[i];uint32_t here=(uint32_t)b.size;if(!mulib_typecore_byte(&b,139))goto fail;for(int k=0;k<10;k++)if(!mulib_dvi_u32_(&b,0))goto fail;if(!mulib_dvi_u32_(&b,prev)||!mulib_typecore_byte(&b,132)||!mulib_dvi_u32_(&b,(uint32_t)h[i])||!mulib_dvi_u32_(&b,(uint32_t)w[i])||!mulib_typecore_byte(&b,140))goto fail;prev=here;}
 if(b.size>UINT32_MAX)goto fail;
 uint32_t post=(uint32_t)b.size;if(!mulib_typecore_byte(&b,248)||!mulib_dvi_u32_(&b,prev)||!mulib_dvi_u32_(&b,num)||!mulib_dvi_u32_(&b,den)||!mulib_dvi_u32_(&b,mag)||!mulib_dvi_u32_(&b,max_v)||!mulib_dvi_u32_(&b,max_h)||!mulib_typecore_u16be(&b,0)||!mulib_typecore_u16be(&b,(uint16_t)n)||!mulib_typecore_byte(&b,249)||!mulib_dvi_u32_(&b,post)||!mulib_typecore_byte(&b,2))goto fail;
 for(int i=0;i<4||b.size%4;i++)if(!mulib_typecore_byte(&b,223))goto fail;
 if(!mulib_typecore_append(out,b.data,b.size))goto fail;
 mulib_typecore_free(&b);return 1;
 fail:mulib_typecore_free(&b);return 0;}
#ifdef __cplusplus
}
#endif
#endif
