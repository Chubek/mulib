/* MuLIB graphical extension: retgtk-color. C11, header-only. */
#ifndef MULIB_RETGTK_COLOR_H
#define MULIB_RETGTK_COLOR_H
#include "mulib-retgtk-widget.h"
static inline void mret_color_paint(mulib_retgtk_widget_t *w,mret_paint *p){mgx_fill(p->surface,w->bounds,w->color);}
static const mulib_retgtk_widget_ops mret_color_ops={NULL,NULL,NULL,mret_color_paint,mret_control_event};
static inline mulib_retgtk_widget_t *mulib_retgtk_color(uint64_t id,uint32_t color){mulib_retgtk_widget_t *w=mret_control(id,"",&mret_color_ops);if(w)w->color=color;return w;}
static inline void mulib_retgtk_color_set(mulib_retgtk_widget_t *w,uint32_t c){w->color=c;mulib_retgtk_widget_invalidate(w);mulib_retgtk_signal_emit(&w->signals,w,MRET_CHANGED,&w->color);}
#endif
