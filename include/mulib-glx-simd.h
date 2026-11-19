/* MuLIB graphical extension: glx-simd. C11, header-only. */
#ifndef MULIB_GLX_SIMD_H
#define MULIB_GLX_SIMD_H
#include "mulib-glx-core.h"
/* memcpy path for opaque spans; portable scalar source-over otherwise. */
static inline void mulib_glx_simd_over(uint32_t *d,const uint32_t *s,size_t n){size_t i=0;for(;i+4<=n;i+=4){if((s[i]&s[i+1]&s[i+2]&s[i+3])>>24==255)memcpy(d+i,s+i,16);else for(int j=0;j<4;j++)d[i+j]=mgx_over(s[i+j],d[i+j]);}for(;i<n;i++)d[i]=mgx_over(s[i],d[i]);}
#endif
