/* MuLIB graphical extension: glx-transform. C11, header-only. */
#ifndef MULIB_GLX_TRANSFORM_H
#define MULIB_GLX_TRANSFORM_H
#include "mulib-glx-core.h"
typedef struct {mgx_affine v[64];size_t n;} mulib_glx_transform_t;
static inline mgx_affine mulib_glx_transform_identity(void){return (mgx_affine){1,0,0,1,0,0};}
static inline mgx_affine mulib_glx_transform_current(const mulib_glx_transform_t *s){return s->n?s->v[s->n-1]:mulib_glx_transform_identity();}
static inline int mulib_glx_transform_push(mulib_glx_transform_t *s,mgx_affine m){if(s->n==64)return 0;mgx_affine v=mgx_mul(mulib_glx_transform_current(s),m);s->v[s->n++]=v;return 1;}
static inline int mulib_glx_transform_pop(mulib_glx_transform_t *s){if(!s->n)return 0;--s->n;return 1;}
static inline int mulib_glx_transform_inverse(mgx_affine m,mgx_affine *out){double d=m.a*m.d-m.b*m.c;if(!isfinite(d)||fabs(d)<1e-15||!out)return 0;*out=(mgx_affine){m.d/d,-m.b/d,-m.c/d,m.a/d,(m.c*m.f-m.d*m.e)/d,(m.b*m.e-m.a*m.f)/d};return 1;}
#endif
