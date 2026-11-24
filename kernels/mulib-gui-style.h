/* MuLIB graphical extension: gui-style. C11, header-only. */
#ifndef MULIB_GUI_STYLE_H
#define MULIB_GUI_STYLE_H
#include "mulib-gui-theme.h"
typedef struct{mgui_theme themes[32];size_t n;}mulib_gui_style_t;
static inline mgui_theme mulib_gui_style_current(const mulib_gui_style_t *s){return s->n?s->themes[s->n-1]:mgui_default_theme();}
static inline int mulib_gui_style_push(mulib_gui_style_t *s,mgui_theme t){if(s->n==32||t.row<=0||t.gap<0||t.padding<0)return 0;s->themes[s->n++]=t;return 1;}
static inline int mulib_gui_style_pop(mulib_gui_style_t *s){if(!s->n)return 0;s->n--;return 1;}
#endif
