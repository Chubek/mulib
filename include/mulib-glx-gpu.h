/* MuLIB graphical extension: glx-gpu. C11, header-only. */
#ifndef MULIB_GLX_GPU_H
#define MULIB_GLX_GPU_H
#include "mulib-glx-drawlist.h"
typedef struct{void *user;int (*upload)(void *,const mgx_surface *,uint64_t *);int (*draw)(void *,const mulib_glx_drawlist_t *,mgx_surface *);void (*destroy_texture)(void *,uint64_t);}mulib_glx_gpu_t;
/* This is a caller-supplied renderer vtable, not a detected GPU capability.
 * With no callback, the canonical CPU renderer reports its real result. */
static inline int
mulib_glx_gpu_draw(mulib_glx_gpu_t *g,const mulib_glx_drawlist_t *d,mgx_surface *s)
{
 if(!mulib_glx_drawlist_valid(d)||!mgx_surface_valid(s))return 0;
 if(g&&g->draw)return g->draw(g->user,d,s);
 return mulib_glx_drawlist_render_checked(d,s);
}
#endif
