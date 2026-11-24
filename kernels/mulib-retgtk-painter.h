/* MuLIB graphical extension: retgtk-painter. C11, header-only. */
#ifndef MULIB_RETGTK_PAINTER_H
#define MULIB_RETGTK_PAINTER_H
#include "mulib-retgtk-widget.h"
static inline void mulib_retgtk_painter_begin(mret_paint *p,mgx_surface *surface,const mulib_glx_font_t *font,mgui_theme theme){*p=(mret_paint){surface,font,theme};}
static inline void mulib_retgtk_painter_rect(mret_paint *p,mgx_rect r,uint32_t color){mgx_fill(p->surface,r,color);}
static inline void mulib_retgtk_painter_text(mret_paint *p,mgx_rect r,const char *s){if(p->font&&s)mulib_glx_text_draw(p->surface,p->font,r.x,r.y,s,strlen(s),p->theme.font_size,p->theme.text);}
#endif
