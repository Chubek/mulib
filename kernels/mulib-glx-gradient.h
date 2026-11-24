/* MuLIB graphical extension: glx-gradient. C11, header-only. */
#ifndef MULIB_GLX_GRADIENT_H
#define MULIB_GLX_GRADIENT_H
#include "mulib-glx-core.h"
typedef struct {double t;uint32_t rgba;} mulib_glx_gradient_stop_t;
static inline uint32_t mulib_glx_gradient_mix(uint32_t a,uint32_t b,double t){if(!isfinite(t))t=0;t=mgx_clamp(t,0,1);uint32_t c=0;for(int k=0;k<=24;k+=8)c|=(uint32_t)(mgx_ch(a,k)*(1-t)+mgx_ch(b,k)*t+.5)<<k;return c;}
static inline uint32_t mulib_glx_gradient_sample(const mulib_glx_gradient_stop_t *s,size_t n,double t){if(!s||!n)return 0;if(t<=s[0].t)return s[0].rgba;for(size_t i=1;i<n;i++)if(t<=s[i].t){double d=s[i].t-s[i-1].t;return mulib_glx_gradient_mix(s[i-1].rgba,s[i].rgba,d>0?(t-s[i-1].t)/d:1);}return s[n-1].rgba;}
static inline void mulib_glx_gradient_linear(mgx_surface *s,mgx_rect r,mgx_point a,mgx_point b,const mulib_glx_gradient_stop_t *st,size_t n){if(!s)return;double dx=b.x-a.x,dy=b.y-a.y,d=dx*dx+dy*dy;r=mgx_intersect(s->clip,r);for(int y=r.y;y<r.y+r.h;y++)for(int x=r.x;x<r.x+r.w;x++)mgx_pixel(s,x,y,mulib_glx_gradient_sample(st,n,d?((x+.5-a.x)*dx+(y+.5-a.y)*dy)/d:0));}
#endif
