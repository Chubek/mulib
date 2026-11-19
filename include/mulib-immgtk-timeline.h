/* MuLIB graphical extension: immgtk-timeline. C11, header-only. */
#ifndef MULIB_IMMGTK_TIMELINE_H
#define MULIB_IMMGTK_TIMELINE_H
#include "mulib-immgtk-native.h"
typedef struct{double start,end,playhead;mgx_rect bounds;}mig_timeline;
static inline double mulib_immgtk_timeline_x(const mig_timeline *t,double time){return t->end>t->start?t->bounds.x+(time-t->start)/(t->end-t->start)*t->bounds.w:t->bounds.x;}
static inline double mulib_immgtk_timeline_time(const mig_timeline *t,double x){return t->bounds.w>0?t->start+(t->end-t->start)*mgx_clamp((x-t->bounds.x)/t->bounds.w,0,1):t->start;}
static inline int mulib_immgtk_timeline(mulib_immgtk_native_t *g,const char *id,mig_timeline *t){return mig_slider(g,id,t->bounds,&t->playhead,t->start,t->end);}
#endif
