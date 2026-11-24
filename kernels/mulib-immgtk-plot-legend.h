/* MuLIB graphical extension: immgtk-plot-legend. C11, header-only. */
#ifndef MULIB_IMMGTK_PLOT_LEGEND_H
#define MULIB_IMMGTK_PLOT_LEGEND_H
#include "mulib-immgtk-native.h"
static inline int mulib_immgtk_plot_legend(mulib_immgtk_native_t *g,const char *const *names,const uint32_t *colors,int *visible,size_t n,mgx_rect r){int c=0;for(size_t i=0;i<n&&i<(size_t)mgx_max(0,r.h/20);i++){mgx_rect b={r.x,r.y+(int)i*20,r.w,20};c|=mig_checkbox(g,names[i],names[i],b,visible+i);mig_rect(g,(mgx_rect){b.x,b.y+4,4,12},colors[i]);}return c;}
#endif
