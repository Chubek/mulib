/* MuLIB graphical extension: retgtk-notebook. C11, header-only. */
#ifndef MULIB_RETGTK_NOTEBOOK_H
#define MULIB_RETGTK_NOTEBOOK_H
#include "mulib-retgtk-stack.h"
static inline mulib_retgtk_widget_t *mulib_retgtk_notebook(uint64_t id){return mulib_retgtk_stack(id);}
static inline int mulib_retgtk_notebook_append(mulib_retgtk_widget_t *w,mulib_retgtk_widget_t *child){if(!mulib_retgtk_widget_add(w,child))return 0;child->visible=w->n==1;return 1;}
static inline int mulib_retgtk_notebook_select(mulib_retgtk_widget_t *w,size_t index){return mulib_retgtk_stack_show(w,index);}
#endif
