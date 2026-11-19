/* MuLIB graphical extension: retgtk-scroll. C11, header-only. */
#ifndef MULIB_RETGTK_SCROLL_H
#define MULIB_RETGTK_SCROLL_H
#include "mulib-retgtk-widget.h"
static inline void mret_scroll_arrange(mulib_retgtk_widget_t *w,mgx_rect r){w->value=mgx_clamp(w->value,0,fmax(0,w->hi-r.h));for(size_t i=0;i<w->n;i++)mulib_retgtk_widget_arrange(w->children[i],(mgx_rect){r.x,r.y-(int)w->value,r.w,mgx_max(r.h,mgx_int(w->hi))});}
static inline int mret_scroll_event(mulib_retgtk_widget_t *w,const mgx_event *e){if(e->type!=MGX_EVENT_SCROLL)return 0;w->value=mgx_clamp(w->value-e->dy*30,0,fmax(0,w->hi-w->bounds.h));mulib_retgtk_widget_invalidate(w);return 1;}
static const mulib_retgtk_widget_ops mret_scroll_ops={NULL,NULL,mret_scroll_arrange,NULL,mret_scroll_event};
static inline mulib_retgtk_widget_t *mulib_retgtk_scroll(uint64_t id,int content){mulib_retgtk_widget_t *w=mulib_retgtk_widget_create(id,&mret_scroll_ops);if(w)w->hi=mgx_max(0,content);return w;}
#endif
