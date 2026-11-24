/* MuLIB graphical extension: retgtk-textview. C11, header-only. */
#ifndef MULIB_RETGTK_TEXTVIEW_H
#define MULIB_RETGTK_TEXTVIEW_H
#include "mulib-retgtk-entry.h"
static inline mulib_retgtk_widget_t *mulib_retgtk_textview(uint64_t id,const char *text){mulib_retgtk_widget_t *w=mulib_retgtk_entry(id,text);if(w)w->desired.h=160;return w;}
static inline mulib_texteditor_t *mulib_retgtk_textview_editor(mulib_retgtk_widget_t *w){return w&&w->kind==MRET_KIND_ENTRY?w->data:NULL;}
#endif
