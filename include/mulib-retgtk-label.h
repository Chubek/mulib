/* MuLIB graphical extension: retgtk-label. C11, header-only. */
#ifndef MULIB_RETGTK_LABEL_H
#define MULIB_RETGTK_LABEL_H
#include "mulib-retgtk-widget.h"
#include "mulib-retgtk-kind.h"
static const mulib_retgtk_widget_ops mret_label_ops={NULL,NULL,NULL,mret_text_paint,NULL};
static inline mulib_retgtk_widget_t *mulib_retgtk_label(uint64_t id,const char *text){mulib_retgtk_widget_t *w=mret_control(id,text,&mret_label_ops);if(w){w->focusable=0;w->kind=MRET_KIND_LABEL;}return w;}
#endif
