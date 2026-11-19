/* MuLIB graphical extension: retgtk-menu. C11, header-only. */
#ifndef MULIB_RETGTK_MENU_H
#define MULIB_RETGTK_MENU_H
#include "mulib-retgtk-box.h"
#include "mulib-retgtk-button.h"
static inline mulib_retgtk_widget_t *mulib_retgtk_menu(uint64_t id,int horizontal){return mulib_retgtk_box(id,!horizontal,2,2);}
static inline mulib_retgtk_widget_t *mulib_retgtk_menu_item(mulib_retgtk_widget_t *menu,uint64_t id,const char *label,mret_signal_fn action,void *u){mulib_retgtk_widget_t *w=mulib_retgtk_button(id,label);if(!w)return NULL;if((action&&!mulib_retgtk_signal_connect(&w->signals,MRET_CLICK,action,u))||!mulib_retgtk_widget_add(menu,w)){mulib_retgtk_widget_unref(w);return NULL;}mulib_retgtk_widget_unref(w);return w;}
#endif
