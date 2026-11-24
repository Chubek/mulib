/* MuLIB graphical extension: retgtk-container. C11, header-only. */
#ifndef MULIB_RETGTK_CONTAINER_H
#define MULIB_RETGTK_CONTAINER_H
#include "mulib-retgtk-widget.h"
static inline int mulib_retgtk_container_append(mulib_retgtk_widget_t *p,mulib_retgtk_widget_t *c){return mulib_retgtk_widget_add(p,c);}
static inline void mulib_retgtk_container_clear(mulib_retgtk_widget_t *p){if(p)while(p->n)mulib_retgtk_widget_remove(p,p->children[p->n-1]);}
#endif
