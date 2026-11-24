/* MuLIB graphical extension: retgtk-stack. C11, header-only. */
#ifndef MULIB_RETGTK_STACK_H
#define MULIB_RETGTK_STACK_H
#include "mulib-retgtk-widget.h"
static inline int mulib_retgtk_stack_show(mulib_retgtk_widget_t *w,size_t index){if(index>=w->n)return 0;for(size_t i=0;i<w->n;i++)w->children[i]->visible=i==index;mulib_retgtk_widget_invalidate(w);return 1;}
static inline mulib_retgtk_widget_t *mulib_retgtk_stack(uint64_t id){return mulib_retgtk_widget_create(id,NULL);}
#endif
