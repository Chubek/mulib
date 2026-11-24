/* MuLIB graphical extension: gui-animation. C11, header-only. */
#ifndef MULIB_GUI_ANIMATION_H
#define MULIB_GUI_ANIMATION_H
#include "mulib-gui-core.h"
typedef struct{double from,to,start,duration;int easing;}mulib_gui_animation_t;
static inline double mulib_gui_animation_value(const mulib_gui_animation_t *a,double time){double t=a->duration>0?mgx_clamp((time-a->start)/a->duration,0,1):1;if(a->easing==1)t=t*t*(3-2*t);else if(a->easing==2)t=1-(1-t)*(1-t)*(1-t);return a->from+(a->to-a->from)*t;}
#endif
