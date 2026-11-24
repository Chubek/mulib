/* MuLIB graphical extension: glx-shape. C11, header-only. */
#ifndef MULIB_GLX_SHAPE_H
#define MULIB_GLX_SHAPE_H
#include "mulib-glx-core.h"
static inline void mulib_glx_shape_ellipse(mgx_surface *s,double x,double y,double rx,double ry,uint32_t c){if(!s||rx<=0||ry<=0||!isfinite(rx+ry+x+y))return;mgx_rect r=mgx_intersect(s->clip,(mgx_rect){mgx_int(floor(x-rx-1)),mgx_int(floor(y-ry-1)),mgx_int(ceil(2*rx+2)),mgx_int(ceil(2*ry+2))});for(int j=r.y;j<r.y+r.h;j++)for(int i=r.x;i<r.x+r.w;i++){double d=hypot((i+.5-x)/rx,(j+.5-y)/ry);mgx_pixel(s,i,j,mgx_alpha(c,mgx_clamp(.5+(1-d)*fmin(rx,ry),0,1)));}}
static inline void mulib_glx_shape_roundrect(mgx_surface *s,mgx_rect r,double rad,uint32_t c){if(!s||r.w<=0||r.h<=0||!isfinite(rad))return;rad=mgx_clamp(rad,0,fmin(r.w,r.h)/2.);if(rad==0){mgx_fill(s,r,c);return;}mgx_rect q=mgx_intersect(r,s->clip);for(int y=q.y;y<q.y+q.h;y++)for(int x=q.x;x<q.x+q.w;x++){double cx=mgx_clamp(x+.5,r.x+rad,r.x+(double)r.w-rad),cy=mgx_clamp(y+.5,r.y+rad,r.y+(double)r.h-rad);mgx_pixel(s,x,y,mgx_alpha(c,mgx_clamp(rad+.5-hypot(x+.5-cx,y+.5-cy),0,1)));}}
#endif
