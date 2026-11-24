/* MuLIB graphical extension: immgtk-plot-heatmap. C11, header-only. */
#ifndef MULIB_IMMGTK_PLOT_HEATMAP_H
#define MULIB_IMMGTK_PLOT_HEATMAP_H
#include "mulib-immgtk-colorpicker.h"
static inline void mulib_immgtk_plot_heatmap(mulib_immgtk_native_t *g,const double *values,size_t rows,size_t cols,double lo,double hi,mgx_rect r){if(!rows||!cols||rows>SIZE_MAX/cols||hi<=lo)return;for(size_t y=0;y<rows;y++)for(size_t x=0;x<cols;x++){double v=values[y*cols+x];if(!isfinite(v))continue;int a=(int)((double)x*r.w/cols),b=(int)((double)(x+1)*r.w/cols),c=(int)((double)y*r.h/rows),d=(int)((double)(y+1)*r.h/rows);mig_rect(g,(mgx_rect){r.x+a,r.y+c,b-a,d-c},mulib_immgtk_color_hsv((1-mgx_clamp((v-lo)/(hi-lo),0,1))*.66,.9,.9,1));}}
#endif
