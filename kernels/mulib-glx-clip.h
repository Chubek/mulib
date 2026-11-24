/* MuLIB graphical extension: glx-clip. C11, header-only. */
#ifndef MULIB_GLX_CLIP_H
#define MULIB_GLX_CLIP_H
#include "mulib-glx-core.h"
/* Zero initialize. A nonempty stack is bound by contract to one live surface;
 * do not interleave surfaces, resize them, or copy the stack while scopes exist.
 */
typedef struct { mgx_rect saved[64]; size_t n; } mulib_glx_clipstack_t;

static inline int mulib_glx_clip_push(mulib_glx_clipstack_t *c,
                                     mgx_surface *s, mgx_rect r)
{
    if (!c || !mgx_surface_valid(s) || c->n >= 64)
        return 0;
    c->saved[c->n++] = s->clip;
    s->clip = mgx_intersect(mgx_surface_clip(s), r);
    return 1;
}

static inline int mulib_glx_clip_pop(mulib_glx_clipstack_t *c, mgx_surface *s)
{
    if (!c || !mgx_surface_valid(s) || !c->n || c->n > 64)
        return 0;
    s->clip = c->saved[--c->n];
    return 1;
}
#endif
