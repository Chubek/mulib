/* MuLIB graphical extension: immgtk-curveeditor. C11, header-only. */
#ifndef MULIB_IMMGTK_CURVEEDITOR_H
#define MULIB_IMMGTK_CURVEEDITOR_H
#include "mulib-immgtk-gizmo.h"
static inline mgx_point mulib_immgtk_curve_eval(const mgx_point p[4],double t){t=mgx_clamp(t,0,1);double u=1-t;return (mgx_point){u*u*u*p[0].x+3*u*u*t*p[1].x+3*u*t*t*p[2].x+t*t*t*p[3].x,u*u*u*p[0].y+3*u*u*t*p[1].y+3*u*t*t*p[2].y+t*t*t*p[3].y};}
static inline int mulib_immgtk_curveeditor(mulib_immgtk_native_t *g,mgx_point p[4]){if(!g||!p)return 0;int c=0;for(int i=0;i<4;i++){char key[16];snprintf(key,sizeof key,"point%d",i);c|=mulib_immgtk_gizmo_translate(g,key,p+i,0);}mgx_point a=p[0];for(int i=1;i<=64;i++){mgx_point b=mulib_immgtk_curve_eval(p,(double)i/64);mgx_drawcmd cmd={0};cmd.kind=MGX_LINE;cmd.a=a;cmd.b=b;cmd.width=2;cmd.color=g->theme.accent;cmd.clip=g->clip;if(!mig_draw_add(g,cmd))return c;a=b;}return c;}
#endif
