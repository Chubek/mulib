/* MuLIB graphical extension: retgtk-statusbar. C11, header-only. */
#ifndef MULIB_RETGTK_STATUSBAR_H
#define MULIB_RETGTK_STATUSBAR_H
#include "mulib-retgtk-label.h"
static inline mulib_retgtk_widget_t *mulib_retgtk_statusbar(uint64_t id,const char *s){return mulib_retgtk_label(id,s);}
static inline int mulib_retgtk_statusbar_message(mulib_retgtk_widget_t *w,const char *s){return mulib_retgtk_widget_text(w,s);}
#endif
