/** @file mulib-pageframe.h
 * @brief Page margins and usable frame calculation.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_PAGEFRAME once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-pageframe.md for scope and limitations.
 */
#ifndef MULIB_PAGEFRAME_H
#define MULIB_PAGEFRAME_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_pageframe_rect { double x,y,width,height; } mulib_pageframe_rect_t;
int mulib_pageframe_inner(double page_width,double page_height,double left,double right,double top,double bottom,mulib_pageframe_rect_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PAGEFRAME_H */
#if defined(MULIB_IMPL_PAGEFRAME) && !defined(MULIB_PAGEFRAME_H_IMPLEMENTED)
#define MULIB_PAGEFRAME_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pageframe_inner(double pw,double ph,double l,double r,double t,double b,mulib_pageframe_rect_t *out){if(!out||!isfinite(pw)||!isfinite(ph)||!isfinite(l)||!isfinite(r)||!isfinite(t)||!isfinite(b)||pw<=0||ph<=0||l<0||r<0||t<0||b<0||l+r>pw||t+b>ph)return 0;*out=(mulib_pageframe_rect_t){l,t,pw-l-r,ph-t-b};return 1;}
#ifdef __cplusplus
}
#endif
#endif
