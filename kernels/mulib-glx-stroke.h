/* MuLIB graphical extension: glx-stroke. C11, header-only. */
#ifndef MULIB_GLX_STROKE_H
#define MULIB_GLX_STROKE_H
#include "mulib-glx-path.h"
static inline void mulib_glx_stroke_path(mgx_surface *s,const mulib_glx_path_t *p,double width,uint32_t color){if(!p)return;for(size_t i=1;i<p->n;i++)if(!p->move[i])mgx_line(s,p->p[i-1],p->p[i],width,color);}
static inline void mulib_glx_stroke_dashed(mgx_surface *s,mgx_point a,mgx_point b,double width,double dash,double gap,uint32_t c){double n=hypot(b.x-a.x,b.y-a.y);if(!isfinite(n)||!isfinite(dash+gap)||dash<=0||gap<0||dash+gap<1e-4)return;for(double t=0;t<n&&t/(dash+gap)<100000;t+=dash+gap){double u=fmin(n,t+dash);mgx_line(s,(mgx_point){a.x+(b.x-a.x)*t/n,a.y+(b.y-a.y)*t/n},(mgx_point){a.x+(b.x-a.x)*u/n,a.y+(b.y-a.y)*u/n},width,c);}}
#endif
