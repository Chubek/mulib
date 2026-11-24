/* MuLIB graphical extension: retgtk-flex. C11, header-only. */
#ifndef MULIB_RETGTK_FLEX_H
#define MULIB_RETGTK_FLEX_H
#include "mulib-retgtk-box.h"
static inline void mulib_retgtk_flex_weight(mulib_retgtk_widget_t *w,double grow){if(w&&isfinite(grow)&&grow>=0){w->weight=grow;mulib_retgtk_widget_invalidate(w);}}
static inline mulib_retgtk_widget_t *mulib_retgtk_flex(uint64_t id,int column,int gap){return mulib_retgtk_box(id,column,gap,0);}
#endif
