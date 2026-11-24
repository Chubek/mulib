/* MuLIB graphical extension: retgtk-delegate. C11, header-only. */
#ifndef MULIB_RETGTK_DELEGATE_H
#define MULIB_RETGTK_DELEGATE_H
#include "mulib-retgtk-model.h"
typedef struct{void (*paint)(void *,mret_paint *,mgx_rect,const char *,int);int (*edit)(void *,mulib_retgtk_model_t *,size_t,size_t,const char *);void *user;}mulib_retgtk_delegate_t;
static inline void mulib_retgtk_delegate_paint(const mulib_retgtk_delegate_t *d,mret_paint *p,mgx_rect r,const char *value,int selection){if(d&&d->paint){d->paint(d->user,p,r,value,selection);return;}mgx_fill(p->surface,r,selection?p->theme.active:p->theme.panel);if(p->font&&value)mulib_glx_text_draw(p->surface,p->font,r.x+4,r.y+2,value,strlen(value),p->theme.font_size,p->theme.text);}
#endif
