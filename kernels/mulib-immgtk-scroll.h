/* MuLIB graphical extension: immgtk-scroll. C11, header-only. */
#ifndef MULIB_IMMGTK_SCROLL_H
#define MULIB_IMMGTK_SCROLL_H
#include "mulib-immgtk-native.h"
typedef struct{mgx_rect previous,view;double offset,content;}mulib_immgtk_scroll_t;
static inline int mulib_immgtk_scroll_begin(mulib_immgtk_native_t *g,mulib_immgtk_scroll_t *s,mgx_rect r,double content){if(!isfinite(content)||content<0)return 0;s->previous=g->clip;s->view=r;s->content=content;if(mgx_contains(r,g->x,g->y))s->offset-=g->scroll*30;s->offset=mgx_clamp(s->offset,0,fmax(0,content-r.h));g->clip=mgx_intersect(g->clip,r);return 1;}
static inline void mulib_immgtk_scroll_end(mulib_immgtk_native_t *g,const mulib_immgtk_scroll_t *s){g->clip=s->previous;}
#endif
