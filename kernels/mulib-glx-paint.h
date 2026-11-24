/* MuLIB graphical extension: glx-paint. C11, header-only. */
#ifndef MULIB_GLX_PAINT_H
#define MULIB_GLX_PAINT_H
#include "mulib-glx-gradient.h"
typedef enum {MULIB_GLX_SOLID,MULIB_GLX_LINEAR,MULIB_GLX_RADIAL,MULIB_GLX_CONIC} mulib_glx_paint_kind;
typedef struct {mulib_glx_paint_kind kind;uint32_t color;mgx_point a,b;const mulib_glx_gradient_stop_t *stops;size_t n;} mulib_glx_paint_t;
static inline uint32_t mulib_glx_paint_sample(const mulib_glx_paint_t *p,double x,double y){if(!p)return 0;double dx=p->b.x-p->a.x,dy=p->b.y-p->a.y,d=dx*dx+dy*dy,t=0;switch(p->kind){case MULIB_GLX_SOLID:return p->color;case MULIB_GLX_LINEAR:t=d?((x-p->a.x)*dx+(y-p->a.y)*dy)/d:0;break;case MULIB_GLX_RADIAL:t=d?hypot(x-p->a.x,y-p->a.y)/sqrt(d):0;break;case MULIB_GLX_CONIC:t=(atan2(y-p->a.y,x-p->a.x)-atan2(dy,dx))/6.283185307179586;t-=floor(t);break;}return mulib_glx_gradient_sample(p->stops,p->n,t);}
static inline void mulib_glx_paint_rect(mgx_surface *s,mgx_rect r,const mulib_glx_paint_t *p){if(!s)return;r=mgx_intersect(r,s->clip);for(int y=r.y;y<r.y+r.h;y++)for(int x=r.x;x<r.x+r.w;x++)mgx_pixel(s,x,y,mulib_glx_paint_sample(p,x+.5,y+.5));}
#endif
