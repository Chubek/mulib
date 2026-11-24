/* MuLIB graphical extension: retgtk-slider. C11, header-only. */
#ifndef MULIB_RETGTK_SLIDER_H
#define MULIB_RETGTK_SLIDER_H
#include "mulib-retgtk-widget.h"
static inline void mret_slider_paint(mulib_retgtk_widget_t *w,mret_paint *p){mret_control_paint(w,p);double t=w->hi>w->lo?mgx_clamp((w->value-w->lo)/(w->hi-w->lo),0,1):0;mgx_fill(p->surface,(mgx_rect){w->bounds.x,w->bounds.y+w->bounds.h/2,(int)(w->bounds.w*t),mgx_max(1,w->bounds.h/2)},p->theme.accent);}
static inline int mret_slider_event(mulib_retgtk_widget_t *w,const mgx_event *e){if(e->type==MGX_EVENT_BUTTON&&e->code==0)w->pressed=e->down;if(w->pressed&&(e->type==MGX_EVENT_MOTION||e->type==MGX_EVENT_BUTTON)&&w->bounds.w>0){w->value=w->lo+(w->hi-w->lo)*mgx_clamp((e->x-w->bounds.x)/w->bounds.w,0,1);mulib_retgtk_widget_invalidate(w);mulib_retgtk_signal_emit(&w->signals,w,MRET_CHANGED,&w->value);return 1;}return e->type==MGX_EVENT_BUTTON;}
static const mulib_retgtk_widget_ops mret_slider_ops={NULL,NULL,NULL,mret_slider_paint,mret_slider_event};
static inline mulib_retgtk_widget_t *mulib_retgtk_slider(uint64_t id,double lo,double hi,double value){if(!isfinite(lo+hi+value)||hi<=lo)return NULL;mulib_retgtk_widget_t *w=mret_control(id,"",&mret_slider_ops);if(w){w->lo=lo;w->hi=hi;w->value=mgx_clamp(value,lo,hi);}return w;}
#endif
