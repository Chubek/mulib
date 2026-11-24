/* MuLIB graphical extension: glx-tiledraster. C11, header-only. */
#ifndef MULIB_GLX_TILEDRASTER_H
#define MULIB_GLX_TILEDRASTER_H
#include "mulib-glx-drawlist.h"
static inline void mulib_glx_tiledraster_draw(mgx_surface *s,const mulib_glx_drawlist_t *d,int tile){if(!s||!d||tile<=0)return;mgx_rect old=s->clip;for(int64_t y=old.y;y<(int64_t)old.y+old.h;y+=tile)for(int64_t x=old.x;x<(int64_t)old.x+old.w;x+=tile){s->clip=mgx_intersect(old,(mgx_rect){(int)x,(int)y,tile,tile});mulib_glx_drawlist_render(d,s);}s->clip=old;}
#endif
