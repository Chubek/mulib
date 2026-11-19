/* MuLIB graphical extension: retgtk-invalidate. C11, header-only. */
#ifndef MULIB_RETGTK_INVALIDATE_H
#define MULIB_RETGTK_INVALIDATE_H
#include "mulib-retgtk-widget.h"
#include "mulib-glx-region.h"
static inline int mulib_retgtk_invalidate_collect(mulib_retgtk_widget_t *w,mulib_glx_region_t *r){if(!w)return 1;if(w->dirty)return mulib_glx_region_add(r,w->bounds);for(size_t i=0;i<w->n;i++)if(!mulib_retgtk_invalidate_collect(w->children[i],r))return 0;return 1;}
#endif
