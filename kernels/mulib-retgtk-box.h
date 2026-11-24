/* MuLIB graphical extension: retgtk-box. C11, header-only. */
#ifndef MULIB_RETGTK_BOX_H
#define MULIB_RETGTK_BOX_H
#include "mulib-retgtk-layout.h"
static inline void mret_box_arrange(mulib_retgtk_widget_t *w,mgx_rect r){mulib_retgtk_layout_linear(w,r,w->kind!=0,mgx_int(w->lo),mgx_int(w->hi));}
static inline void mret_box_measure(mulib_retgtk_widget_t *w,mulib_gui_constraints_t c,mulib_gui_size_t *s){mulib_retgtk_layout_linear_measure(w,c,s,w->kind!=0,mgx_int(w->lo),mgx_int(w->hi));}
static const mulib_retgtk_widget_ops mret_box_ops={NULL,mret_box_measure,mret_box_arrange,NULL,NULL};
static inline mulib_retgtk_widget_t *mulib_retgtk_box(uint64_t id,int vertical,int gap,int padding){mulib_retgtk_widget_t *w=mulib_retgtk_widget_create(id,&mret_box_ops);if(w){w->kind=vertical!=0;w->lo=mgx_max(0,gap);w->hi=mgx_max(0,padding);}return w;}
#endif
