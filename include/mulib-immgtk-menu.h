/* MuLIB graphical extension: immgtk-menu. C11, header-only. */
#ifndef MULIB_IMMGTK_MENU_H
#define MULIB_IMMGTK_MENU_H
#include "mulib-immgtk-native.h"
static inline int mulib_immgtk_menu_item(mulib_immgtk_native_t *g,const char *label,mgx_rect r,int enabled,int checked){int click=mig_button(g,label,label,r,enabled);if(checked)mig_rect(g,(mgx_rect){r.x+r.w-12,r.y+6,6,6},g->theme.accent);return click;}
#endif
