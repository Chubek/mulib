/* MuLIB graphical extension: retgtk-progress. C11, header-only. */
#ifndef MULIB_RETGTK_PROGRESS_H
#define MULIB_RETGTK_PROGRESS_H
#include "mulib-retgtk-slider.h"
static const mulib_retgtk_widget_ops mret_progress_ops={NULL,NULL,NULL,mret_slider_paint,NULL};
static inline mulib_retgtk_widget_t *mulib_retgtk_progress(uint64_t id,double value){mulib_retgtk_widget_t *w=mulib_retgtk_widget_create(id,&mret_progress_ops);if(w)w->value=mgx_clamp(value,0,1);return w;}
#endif
