/* MuLIB graphical extension: immgtk-plot-series. C11, header-only. */
#ifndef MULIB_IMMGTK_PLOT_SERIES_H
#define MULIB_IMMGTK_PLOT_SERIES_H
#include "mulib-immgtk-plot-axis.h"
static inline void mulib_immgtk_plot_series(mulib_immgtk_native_t *g,const double *x,const double *y,size_t n,mig_axis ax,mig_axis ay,mgx_rect r,uint32_t c,int scatter){if(!g||(!y&&n))return;mgx_point prior={0,0};int valid=0;for(size_t i=0;i<n;i++){double u=mulib_immgtk_plot_axis_map(ax,x?x[i]:(double)i),v=mulib_immgtk_plot_axis_map(ay,y[i]);if(!isfinite(u+v)){valid=0;continue;}mgx_point p={r.x+u*r.w,r.y+(1-v)*r.h};if(scatter||valid){mgx_drawcmd cmd={0};cmd.kind=scatter?MGX_CIRCLE:MGX_LINE;cmd.a=scatter?p:prior;cmd.b=p;cmd.width=scatter?2:1.5;cmd.clip=mgx_intersect(g->clip,r);cmd.color=c;if(!mig_draw_add(g,cmd))return;}prior=p;valid=1;}}
#endif
