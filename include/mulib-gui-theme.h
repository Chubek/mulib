/* MuLIB graphical extension: gui-theme. C11, header-only. */
#ifndef MULIB_GUI_THEME_H
#define MULIB_GUI_THEME_H
#include "mulib-gui-core.h"
typedef mgui_theme mulib_gui_theme_t;
static inline mgui_theme mulib_gui_theme_dark(void){return mgui_default_theme();}
static inline mgui_theme mulib_gui_theme_light(void){return (mgui_theme){0xfff2f4f7,0xffffffff,0xffe2e7ed,0xffd1dbe7,0xffa9c8ed,0xff202630,0xff286bb0,0xff929da9,8,4,24,14,4};}
#endif
