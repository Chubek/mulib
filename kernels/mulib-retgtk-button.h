/* MuLIB graphical extension: retgtk-button. C11, header-only. */
#ifndef MULIB_RETGTK_BUTTON_H
#define MULIB_RETGTK_BUTTON_H
#include "mulib-retgtk-widget.h"
#include "mulib-retgtk-kind.h"
static inline mulib_retgtk_widget_t *mulib_retgtk_button(uint64_t id,const char *s){mulib_retgtk_widget_t *w=mret_control(id,s,NULL);if(w)w->kind=MRET_KIND_BUTTON;return w;}
static inline void mret_toggle_clicked(void *u,void *sender,unsigned sig,const void *data){(void)u;(void)sig;(void)data;mulib_retgtk_widget_t *w=sender;w->value=!w->value;mulib_retgtk_widget_invalidate(w);mulib_retgtk_signal_emit(&w->signals,w,MRET_CHANGED,&w->value);}
static inline void mret_toggle_paint(mulib_retgtk_widget_t *w,mret_paint *p){mret_control_paint(w,p);if(w->value)mgx_fill(p->surface,(mgx_rect){w->bounds.x+w->bounds.w-14,w->bounds.y+5,8,mgx_max(0,w->bounds.h-10)},p->theme.accent);}
static const mulib_retgtk_widget_ops mret_toggle_ops={NULL,NULL,NULL,mret_toggle_paint,mret_control_event};
static inline mulib_retgtk_widget_t *mulib_retgtk_toggle(uint64_t id,const char *s,int on){mulib_retgtk_widget_t *w=mret_control(id,s,&mret_toggle_ops);if(w){w->kind=MRET_KIND_TOGGLE;w->value=!!on;if(!mulib_retgtk_signal_connect(&w->signals,MRET_CLICK,mret_toggle_clicked,NULL)){mulib_retgtk_widget_unref(w);return NULL;}}return w;}
#endif
