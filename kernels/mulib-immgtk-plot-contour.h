/* MuLIB graphical extension: immgtk-plot-contour. C11, header-only. */
#ifndef MULIB_IMMGTK_PLOT_CONTOUR_H
#define MULIB_IMMGTK_PLOT_CONTOUR_H
#include "mulib-glx-core.h"
typedef void (*mig_contour_fn)(void *,mgx_point,mgx_point);
static inline size_t mulib_immgtk_plot_contour(const double *v,size_t w,size_t h,double level,mig_contour_fn emit,void *u){if(!v||!emit||w<2||h<2||w>SIZE_MAX/h)return 0;size_t count=0;for(size_t y=0;y+1<h;y++)for(size_t x=0;x+1<w;x++){double a[4]={v[y*w+x],v[y*w+x+1],v[(y+1)*w+x+1],v[(y+1)*w+x]};mgx_point p[4]={{x,y},{x+1,y},{x+1,y+1},{x,y+1}},out[4];size_t n=0;for(int i=0;i<4;i++){int j=(i+1)%4;if(!isfinite(a[i]+a[j])||(a[i]>=level)==(a[j]>=level))continue;double t=(level-a[i])/(a[j]-a[i]);out[n++]=(mgx_point){p[i].x+t*(p[j].x-p[i].x),p[i].y+t*(p[j].y-p[i].y)};}if(n>=2){emit(u,out[0],out[1]);count++;}if(n==4){emit(u,out[2],out[3]);count++;}}return count;}
#endif
