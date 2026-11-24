/* MuLIB graphical extension: retgtk-image. C11, header-only. */
#ifndef MULIB_RETGTK_IMAGE_H
#define MULIB_RETGTK_IMAGE_H
#include "mulib-retgtk-widget.h"
static inline void mret_image_paint(mulib_retgtk_widget_t *w,mret_paint *p){if(w->data)mulib_glx_texture_draw(p->surface,w->data,w->bounds,1);}
static const mulib_retgtk_widget_ops mret_image_ops={NULL,NULL,NULL,mret_image_paint,NULL};
static inline mulib_retgtk_widget_t *mulib_retgtk_image(uint64_t id,const mgx_surface *image){mulib_retgtk_widget_t *w=mulib_retgtk_widget_create(id,&mret_image_ops);if(w)w->data=(void*)image;return w;}
#endif
