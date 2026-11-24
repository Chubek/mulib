/* MuLIB graphical extension: retgtk-toolbar. C11, header-only. */
#ifndef MULIB_RETGTK_TOOLBAR_H
#define MULIB_RETGTK_TOOLBAR_H
#include "mulib-retgtk-menu.h"
static inline mulib_retgtk_widget_t *mulib_retgtk_toolbar(uint64_t id){return mulib_retgtk_box(id,0,4,4);}
static inline mulib_retgtk_widget_t *mulib_retgtk_toolbar_action(mulib_retgtk_widget_t *w,uint64_t id,const char *s,mret_signal_fn fn,void *u){return mulib_retgtk_menu_item(w,id,s,fn,u);}
#endif
