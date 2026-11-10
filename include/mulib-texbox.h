/** @file mulib-texbox.h
 * @brief TeX-style horizontal/vertical box dimensions with checked packing.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_TEXBOX once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-texbox.md for scope and limitations.
 */
#ifndef MULIB_TEXBOX_H
#define MULIB_TEXBOX_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_texbox_box { int64_t width,height,depth; } mulib_texbox_box_t;
int mulib_texbox_hpack(const mulib_texbox_box_t *children,size_t count,mulib_texbox_box_t *out);
int mulib_texbox_vpack(const mulib_texbox_box_t *children,size_t count,mulib_texbox_box_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TEXBOX_H */
#if defined(MULIB_IMPL_TEXBOX) && !defined(MULIB_TEXBOX_H_IMPLEMENTED)
#define MULIB_TEXBOX_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_texbox_hpack(const mulib_texbox_box_t *b,size_t n,mulib_texbox_box_t *out){if((!b&&n)||!out)return 0;int64_t w=0,h=0,d=0;for(size_t i=0;i<n;i++){if(b[i].width<0||b[i].height<0||b[i].depth<0||w>INT64_MAX-b[i].width)return 0;w+=b[i].width;if(b[i].height>h)h=b[i].height;if(b[i].depth>d)d=b[i].depth;}*out=(mulib_texbox_box_t){w,h,d};return 1;}
int mulib_texbox_vpack(const mulib_texbox_box_t *b,size_t n,mulib_texbox_box_t *out){if((!b&&n)||!out)return 0;int64_t height=0,w=0;for(size_t i=0;i<n;i++){if(b[i].width<0||b[i].height<0||b[i].depth<0||height>INT64_MAX-b[i].height||height+b[i].height>INT64_MAX-b[i].depth)return 0;height+=b[i].height+b[i].depth;if(b[i].width>w)w=b[i].width;}*out=(mulib_texbox_box_t){w,height,0};return 1;}
#ifdef __cplusplus
}
#endif
#endif
