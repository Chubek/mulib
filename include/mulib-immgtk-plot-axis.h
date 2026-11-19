/* MuLIB graphical extension: immgtk-plot-axis. C11, header-only. */
#ifndef MULIB_IMMGTK_PLOT_AXIS_H
#define MULIB_IMMGTK_PLOT_AXIS_H
#include "mulib-immgtk-native.h"
typedef struct{double lo,hi;int logarithmic;}mig_axis;
static inline double mulib_immgtk_plot_axis_map(mig_axis a,double x){if(!isfinite(a.lo+a.hi+x)||a.hi<=a.lo||(a.logarithmic&&(x<=0||a.lo<=0)))return NAN;if(a.logarithmic)return log(x/a.lo)/log(a.hi/a.lo);return (x-a.lo)/(a.hi-a.lo);}
static inline double mulib_immgtk_plot_axis_unmap(mig_axis a,double t){return a.logarithmic?a.lo*pow(a.hi/a.lo,t):a.lo+(a.hi-a.lo)*t;}
static inline double mulib_immgtk_plot_axis_step(mig_axis a,unsigned ticks){if(a.hi<=a.lo||!ticks)return 0;double step=(a.hi-a.lo)/ticks,d=pow(10,floor(log10(step))),k=step/d;return d*(k<=1?1:k<=2?2:k<=5?5:10);}
#endif
