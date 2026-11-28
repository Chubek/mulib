/* MuLIB browser staging addition: css-transform
 * Implemented subset: 2D affine composition and point transformation.
 * Remaining: 3D matrices, transform lists, transform origin and interpolation.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSS_TRANSFORM_H
#define MULIB_CSS_TRANSFORM_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {double a,b,c,d,e,f;} mulib_css_transform_matrix;
static inline mulib_css_transform_matrix mulib_css_transform_multiply(mulib_css_transform_matrix x,mulib_css_transform_matrix y){return (mulib_css_transform_matrix){x.a*y.a+x.c*y.b,x.b*y.a+x.d*y.b,x.a*y.c+x.c*y.d,x.b*y.c+x.d*y.d,x.a*y.e+x.c*y.f+x.e,x.b*y.e+x.d*y.f+x.f};}
static inline void mulib_css_transform_point(mulib_css_transform_matrix m,double x,double y,double *ox,double *oy){if(ox&&oy){double a=m.a*x+m.c*y+m.e,b=m.b*x+m.d*y+m.f;*ox=a;*oy=b;}}
#endif /* MULIB_CSS_TRANSFORM_H */
