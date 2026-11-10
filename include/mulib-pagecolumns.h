/** @file mulib-pagecolumns.h
 * @brief Even column rectangles with configurable width and gap.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_PAGECOLUMNS once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-pagecolumns.md for scope and limitations.
 */
#ifndef MULIB_PAGECOLUMNS_H
#define MULIB_PAGECOLUMNS_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_pagecolumns_rect { double x,y,width,height; } mulib_pagecolumns_rect_t;
int mulib_pagecolumns_divide(mulib_pagecolumns_rect_t area,unsigned columns,double gap,mulib_pagecolumns_rect_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PAGECOLUMNS_H */
#if defined(MULIB_IMPL_PAGECOLUMNS) && !defined(MULIB_PAGECOLUMNS_H_IMPLEMENTED)
#define MULIB_PAGECOLUMNS_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pagecolumns_divide(mulib_pagecolumns_rect_t r,unsigned n,double gap,mulib_pagecolumns_rect_t *out){if(!out||n==0||n>1024||!isfinite(gap)||gap<0||!isfinite(r.width)||r.width<0||!isfinite(r.height)||r.height<0||gap*(n-1)>r.width)return 0;double w=(r.width-gap*(n-1))/n;for(unsigned i=0;i<n;i++)out[i]=(mulib_pagecolumns_rect_t){r.x+i*(w+gap),r.y,w,r.height};return 1;}
#ifdef __cplusplus
}
#endif
#endif
