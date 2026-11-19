/* MuLIB graphical extension: glx-cursor. C11, header-only. */
#ifndef MULIB_GLX_CURSOR_H
#define MULIB_GLX_CURSOR_H
#include "mulib-glx-compositor.h"
typedef struct{const mgx_surface *image;int hot_x,hot_y,x,y,visible;}mulib_glx_cursor_t;
static inline void mulib_glx_cursor_draw(mgx_surface *s,const mulib_glx_cursor_t *c){if(c&&c->visible&&c->image)mulib_glx_composite(s,c->image,c->x-c->hot_x,c->y-c->hot_y,1,MULIB_GLX_OVER);}
#endif
