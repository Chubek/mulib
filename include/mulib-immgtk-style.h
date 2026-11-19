/* MuLIB graphical extension: immgtk-style. C11, header-only. */
#ifndef MULIB_IMMGTK_STYLE_H
#define MULIB_IMMGTK_STYLE_H
#include "mulib-immgtk-native.h"
#include "mulib-gui-style.h"
static inline int mulib_immgtk_style_push(mulib_immgtk_native_t *g,mulib_gui_style_t *s,mgui_theme t){if(!mulib_gui_style_push(s,g->theme))return 0;g->theme=t;return 1;}
static inline int mulib_immgtk_style_pop(mulib_immgtk_native_t *g,mulib_gui_style_t *s){if(!s->n)return 0;g->theme=s->themes[--s->n];return 1;}
#endif
