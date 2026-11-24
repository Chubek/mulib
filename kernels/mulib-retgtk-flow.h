/* MuLIB graphical extension: retgtk-flow. C11, header-only. */
#ifndef MULIB_RETGTK_FLOW_H
#define MULIB_RETGTK_FLOW_H
#include "mulib-retgtk-widget.h"
static inline void mret_flow_arrange(mulib_retgtk_widget_t *w,mgx_rect r){int x=0,y=0,row=0,gap=(int)w->lo;for(size_t i=0;i<w->n;i++){mulib_retgtk_widget_t *c=w->children[i];if(!c->visible)continue;mulib_gui_size_t s=mulib_retgtk_widget_measure(c,(mulib_gui_constraints_t){0,0,r.w,r.h});if(x&&x+s.w>r.w){x=0;y+=row+gap;row=0;}mulib_retgtk_widget_arrange(c,(mgx_rect){r.x+x,r.y+y,s.w,s.h});x+=s.w+gap;row=mgx_max(row,s.h);}}
static const mulib_retgtk_widget_ops mret_flow_ops={NULL,NULL,mret_flow_arrange,NULL,NULL};
static inline mulib_retgtk_widget_t *mulib_retgtk_flow(uint64_t id,int gap){mulib_retgtk_widget_t *w=mulib_retgtk_widget_create(id,&mret_flow_ops);if(w)w->lo=mgx_max(0,gap);return w;}
#endif
