/* MuLIB graphical extension: immgtk-plot-interact. C11, header-only. */
#ifndef MULIB_IMMGTK_PLOT_INTERACT_H
#define MULIB_IMMGTK_PLOT_INTERACT_H
#include "mulib-immgtk-plot-axis.h"
static inline int mulib_immgtk_plot_interact(mulib_immgtk_native_t *g,mgx_rect r,mig_axis *x,mig_axis *y){if(!mgx_contains(r,g->x,g->y)||!g->scroll||r.w<=0||r.h<=0)return 0;double u=(g->x-r.x)/r.w,v=1-(g->y-r.y)/r.h,z=pow(1.15,-mgx_clamp(g->scroll,-20,20));mig_axis a=*x,b=*y;x->lo=mulib_immgtk_plot_axis_unmap(a,u*(1-z));x->hi=mulib_immgtk_plot_axis_unmap(a,u+(1-u)*z);y->lo=mulib_immgtk_plot_axis_unmap(b,v*(1-z));y->hi=mulib_immgtk_plot_axis_unmap(b,v+(1-v)*z);return 1;}
#endif
