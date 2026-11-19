/* MuLIB graphical extension: retgtk-canvas. C11, header-only. */
#ifndef MULIB_RETGTK_CANVAS_H
#define MULIB_RETGTK_CANVAS_H
#include "mulib-retgtk-widget.h"
typedef struct{void (*paint)(void *,mret_paint *,mgx_rect);int (*event)(void *,const mgx_event *);void *user;}mret_canvas;
static inline void mret_canvas_paint(mulib_retgtk_widget_t *w,mret_paint *p){mret_canvas *c=w->data;if(c&&c->paint)c->paint(c->user,p,w->bounds);}
static inline int mret_canvas_event(mulib_retgtk_widget_t *w,const mgx_event *e){mret_canvas *c=w->data;return c&&c->event&&c->event(c->user,e);}
static const mulib_retgtk_widget_ops mret_canvas_ops={NULL,NULL,NULL,mret_canvas_paint,mret_canvas_event};
static inline mulib_retgtk_widget_t *mulib_retgtk_canvas(uint64_t id,mret_canvas *c){mulib_retgtk_widget_t *w=mulib_retgtk_widget_create(id,&mret_canvas_ops);if(w)w->data=c;return w;}
#endif
